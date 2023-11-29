/**
 *
 *
 *
 */

#define AFL_LLVM_PASS

#include "../AFL-Fuzz/config.h"
#include "../AFL-Fuzz/types.h"
#include "../Utility/FGoUtils.hpp"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "json/json.h"
#include <cstdlib>
#include <fstream>
#include <unordered_map>

using namespace llvm;

cl::opt<std::string> finalDistanceDir(
    LLVM_OPT_DISTDIR_NAME, cl::desc("The directory containing the final distance files."),
    cl::value_desc(LLVM_OPT_DISTDIR_NAME)
);

cl::opt<std::string> projRootDir(
    LLVM_OPT_PROJROOT_NAME, cl::desc("The root directory of the project."),
    cl::value_desc(LLVM_OPT_PROJROOT_NAME)
);

namespace
{
class FGoModulePass : public ModulePass
{
public:
    static char ID;
    FGoModulePass() : ModulePass(ID)
    {}

    bool runOnModule(Module &M) override;
};
} // namespace

char FGoModulePass::ID = 0;

namespace FGo
{
/// @brief Get debug location for a LLVM IR instruction
/// @param I
/// @param filePath
/// @param fileName
/// @param line
/// @param pathPrefix
static void getDebugLocWithPath(
    const Instruction *I, std::string &filePath, std::string &fileName, unsigned &line,
    const std::string &pathPrefix
)
{
    filePath = "";
    fileName = "";
    line = 0;

    if (DILocation *Loc = I->getDebugLoc()) {
        line = Loc->getLine();
        std::string Directory = Loc->getDirectory().str();
        std::string Filename = Loc->getFilename().str();

        if (Filename.empty()) {
            DILocation *oDILoc = Loc->getInlinedAt();
            if (oDILoc) {
                line = oDILoc->getLine();
                Directory = oDILoc->getDirectory().str();
                Filename = oDILoc->getFilename().str();
            }
        }

        if (!Filename.empty()) {
            if (Filename.front() == '/') {
                filePath = Filename;
                fileName = sys::path::filename(Filename).str();
            }
            else {
                SmallString<PATH_MAX> absPath(Directory);
                sys::path::append(absPath, Filename);
                filePath = absPath.str().str();
                absPath.clear();
                if (!filePath.empty()) {
                    auto ec = sys::fs::real_path(filePath, absPath);
                    if (!ec) {
                        filePath = absPath.str().str();
                        if (!pathPrefix.empty() && filePath.size() >= pathPrefix.size()) {
                            if (!filePath.compare(0, pathPrefix.size(), pathPrefix)) {
                                filePath = filePath.substr(pathPrefix.size());
                                if (!filePath.empty() && filePath.front() == '/')
                                    filePath = filePath.substr(1);
                            }
                        }
                    }
                }
            }
        }
    }
}

/// @brief Load Json from a file stream
/// @param inStream
/// @param root
/// @return
bool parseJsonValueFromFile(std::ifstream &inStream, Json::Value &root)
{
    Json::CharReaderBuilder builder;
    builder["collectComments"] = true;
    JSONCPP_STRING errs;
    return Json::parseFromStream(builder, inStream, &root, &errs);
}
} // namespace FGo

bool FGoModulePass::runOnModule(Module &M)
{
    using namespace FGo;

    // Preprocessing Mode
    if (finalDistanceDir.empty() && !getenv(DIST_DIR_ENVAR)) {
        if (!getenv("AFL_QUIET")) {
            HighlightSome(COMPILER_HINT, "(Preprocessing Mode - bitcode)");
        }
        return true;
    }

    SmallString<PATH_MAX> basePath;
    if (finalDistanceDir.empty()) {
        finalDistanceDir = getenv(DIST_DIR_ENVAR);
    }
    if (!sys::fs::exists(finalDistanceDir) || !sys::fs::is_directory(finalDistanceDir)) {
        AbortOnError(
            false,
            "The path '" + finalDistanceDir + "' doesn't exist or doesn't point to a directory"
        );
        return false;
    }
    if (projRootDir.empty()) {
        const char *tmpProjRootDir = getenv(PROJ_ROOT_ENVAR);
        if (!tmpProjRootDir) {
            AbortOnError(
                false,
                "Failed to find the root directory of the project from environment variable"
            );
            return false;
        }
        projRootDir = std::string(tmpProjRootDir);
    }
    if (!sys::fs::exists(projRootDir) || !sys::fs::is_directory(projRootDir)) {
        AbortOnError(
            false,
            "The path '" + projRootDir + "' doesn't exist or doesn't point to a directory"
        );
        return false;
    }
    basePath.clear();
    sys::fs::real_path(projRootDir, basePath);
    projRootDir = basePath.str().str();
    if (projRootDir.empty()) {
        AbortOnError(
            false,
            "Unexpected root: failed to get the real path of the root directory of the project"
        );
        return false;
    }

    // Search depth-dirst distance file and backtrace distance file
    basePath = finalDistanceDir;
    sys::path::append(basePath, std::string(FINAL_DISTANCE_FILENAME) + ".json");
    std::string finalDistanceFile = basePath.str().str();
    if (!sys::fs::exists(finalDistanceFile) || !sys::fs::is_regular_file(finalDistanceFile)) {
        AbortOnError(false, "The distance file '" + finalDistanceFile + "' doesn't exist");
        return false;
    }

    // Load json value from files
    std::unordered_map<std::string, std::unordered_map<unsigned, int32_t>> BBDistMap;
    {
        std::ifstream ifs;

        Json::Value jsonRoot;
        ifs.open(finalDistanceFile, std::ios::in);
        if (!ifs.is_open()) {
            AbortOnError(false, "Failed to open distance file " + finalDistanceFile);
            return false;
        }
        if (!parseJsonValueFromFile(ifs, jsonRoot)) {
            AbortOnError(false, "Failed to parse json file " + finalDistanceFile);
            return false;
        }
        ifs.close();

        // Get the BB distances
        Json::Value::Members members = jsonRoot.getMemberNames();
        for (auto iter = members.begin(); iter != members.end(); ++iter) {
            if (jsonRoot[*iter].type() != Json::objectValue) {
                AbortOnError(
                    false, "The json file '" + finalDistanceFile +
                               "' was destroyed. The key '" + *iter + "' is wrong."
                );
                return false;
            }
            Json::Value::Members innerMembers = jsonRoot[*iter].getMemberNames();
            for (auto i_iter = innerMembers.begin(); i_iter != innerMembers.end(); ++i_iter) {
                if (jsonRoot[*iter][*i_iter].type() != Json::intValue) {
                    AbortOnError(
                        false, "The json file '" + finalDistanceFile +
                                   "' was destroyed. The key '" + *i_iter + "' in key '" +
                                   *iter + "' is wrong."
                    );
                    return false;
                }
                unsigned line = std::stoul(*i_iter);
                if (BBDistMap.find(*iter) == BBDistMap.end()) {
                    BBDistMap[*iter] = std::unordered_map<unsigned, int32_t>();
                }

                // TODO
                // Scale the raw distances to the instrumentation distances
                int tmpDistance = jsonRoot[*iter][*i_iter].asInt();
                if (tmpDistance >= 0) BBDistMap[*iter][line] = tmpDistance * 3;
            }
        }
    }
    if (BBDistMap.empty()) {
        AbortOnError(
            false,
            "Failed to find any distance for basic blocks in distance file " + finalDistanceFile
        );
        return false;
    }

    if (!getenv("AFL_QUIET")) {
        HighlightSome(COMPILER_HINT, "(Instrumentation Mode)");
    }

    // =======================
    // Instrument distances

    size_t instrBBCount = 0;

    LLVMContext &C = M.getContext();
    IntegerType *Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType *Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType *Int64Ty = IntegerType::getInt64Ty(C);

#ifdef __x86_64__
    IntegerType *LargestType = Int64Ty;
    ConstantInt *MapCntLoc = ConstantInt::get(LargestType, MAP_SIZE + 8);
#else
    IntegerType *LargestType = Int32Ty;
    ConstantInt *MapCntLoc = ConstantInt::get(LargestType, MAP_SIZE + 4);
#endif
    ConstantInt *MapDistLoc = ConstantInt::get(LargestType, MAP_SIZE);
    ConstantInt *One = ConstantInt::get(LargestType, 1);

    // Get globals for the SHM region and the previous location. Note that
    // __afl_prev_loc is thread-local.
    GlobalVariable *AFLMapPtr = new GlobalVariable(
        M, PointerType::get(Int8Ty, 0), false, GlobalValue::ExternalLinkage, 0, "__afl_area_ptr"
    );
    GlobalVariable *AFLPrevLoc = new GlobalVariable(
        M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc", 0,
        GlobalVariable::GeneralDynamicTLSModel, 0, false
    );

    // Interate
    for (auto &F : M) {

        for (auto &BB : F) {
            int32_t distance = -2;
            std::string bbName = "";

            // Get the location of this basic block and fetch the distance
            for (auto &I : BB) {
                if (bbName.empty()) {
                    std::string filePath, fileName;
                    unsigned line = 0;

                    getDebugLocWithPath(&I, filePath, fileName, line, projRootDir);

                    if (filePath.empty() || line == 0) continue;
                    else bbName = filePath + ":" + std::to_string(line);

                    if (BBDistMap.find(filePath) != BBDistMap.end()) {
                        if (BBDistMap[filePath].find(line) != BBDistMap[filePath].end()) {
                            distance = BBDistMap[filePath][line];
                        }
                    }
                    else if (BBDistMap.find(fileName) != BBDistMap.end()) {
                        if (BBDistMap[fileName].find(line) != BBDistMap[fileName].end()) {
                            distance = BBDistMap[fileName][line];
                        }
                    }
                }
            }

            BasicBlock::iterator IP = BB.getFirstInsertionPt();
            IRBuilder<> IRB(&(*IP));

            // Current location
            unsigned int cur_loc = AFL_R(MAP_SIZE);
            ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);

            // Load previous location
            LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
            PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

            // Load SHM pointer
            LoadInst *MapPtr = IRB.CreateLoad(AFLMapPtr);
            MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *MapPtrIdx = IRB.CreateGEP(MapPtr, IRB.CreateXor(PrevLocCasted, CurLoc));

            // Update bitmap
            LoadInst *Counter = IRB.CreateLoad(MapPtrIdx);
            Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
            IRB.CreateStore(Incr, MapPtrIdx)
                ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            // Set `prev_loc` to `cur_loc >> 1`
            StoreInst *Store =
                IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
            Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            if (distance >= 0) {
                ConstantInt *Distance = ConstantInt::get(LargestType, (unsigned)distance);

                // Add distance to shm[MAPSIZE]

                Value *MapDistPtr = IRB.CreateBitCast(
                    IRB.CreateGEP(MapPtr, MapDistLoc), LargestType->getPointerTo()
                );
                LoadInst *MapDist = IRB.CreateLoad(MapDistPtr);
                MapDist->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                Value *IncrDist = IRB.CreateAdd(MapDist, Distance);
                IRB.CreateStore(IncrDist, MapDistPtr)
                    ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                // Increase count at shm[MAPSIZE + (4 or 8)]

                Value *MapCntPtr = IRB.CreateBitCast(
                    IRB.CreateGEP(MapPtr, MapCntLoc), LargestType->getPointerTo()
                );
                LoadInst *MapCnt = IRB.CreateLoad(MapCntPtr);
                MapCnt->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                Value *IncrCnt = IRB.CreateAdd(MapCnt, One);
                IRB.CreateStore(IncrCnt, MapCntPtr)
                    ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            }
            ++instrBBCount;
        }
    }

    // Some hints
    if (!getenv("AFL_QUIET")) {
        if (instrBBCount == 0) {
            WarnOnError(false, "Failed to find instrumentation targets");
        }
        else {
            SucceedSome(
                "[+]",
                std::string("Instrumented ") + std::to_string(instrBBCount) + " basic blocks"
            );
        }
    }

    return true;
}

// static RegisterPass<FGoModulePass> X("FGo-Module-Pass", "FGo Module Pass", false, false);

// extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo()
// {
//     return {LLVM_PLUGIN_API_VERSION, "FGoModulePass", "v1.0", [](PassBuilder &PB) {
//                 PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM,
//                                                       ArrayRef<PassBuilder::PipelineElement>)
//                                                       {
//                     if (Name == "FGo-Module-Pass") {
//                         MPM.addPass(new FGoModulePass());
//                         return true;
//                     }
//                     return false;
//                 });
//             }};
// }

static void registerFGoPass(const PassManagerBuilder &, legacy::PassManagerBase &PM)
{
    PM.add(new FGoModulePass());
}

static RegisterStandardPasses RegisterFGoPass(
    PassManagerBuilder::EP_OptimizerLast, registerFGoPass
);

static RegisterStandardPasses RegisterFGoPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerFGoPass
);