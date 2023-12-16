/*
   american fuzzy lop++ - LLVM-mode instrumentation pass
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com>,
              Adrian Herrera <adrian.herrera@anu.edu.au>,
              Michal Zalewski

   LLVM integration design comes from Laszlo Szekeres. C bits copied-and-pasted
   from afl-as.c are Michal's fault.

   NGRAM previous location coverage comes from Adrian Herrera.

   Copyright 2015, 2016 Google Inc. All rights reserved.
   Copyright 2019-2023 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     https://www.apache.org/licenses/LICENSE-2.0

   This library is plugged into LLVM when invoking clang through afl-clang-fast.
   It tells the compiler to add code roughly equivalent to the bits discussed
   in ../afl-as.h.

 */

/**
 * Created by Joshua on Nov. 20, 2023
 *
 */

#define AFL_LLVM_PASS

#include "../AFL-Fuzz/config.h"
#include "../AFL-Fuzz/types.h"
#include "../Utility/FGoDefs.h"
#include "../Utility/FGoUtils.hpp"

#include "llvm/Config/llvm-config.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

/* use new pass manager */
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "json/json.h"
#include <cstdlib>
#include <fstream>
#include <unordered_map>

using namespace llvm;

#if LLVM_VERSION_MAJOR >= 14

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
class FGoModulePass : public PassInfoMixin<FGoModulePass>
{
public:
    FGoModulePass()
    {}

    /* use new pass manager */
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};
} // namespace

/* use new pass manager */
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo()
{
    return {
        LLVM_PLUGIN_API_VERSION, "FGoModulePass", "v1.0",
        /* lambda to insert our pass into the pass pipeline. */
        [](PassBuilder &PB) {

    #if 1
            PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM,
                                                  OptimizationLevel OL) {
                MPM.addPass(FGoModulePass());
            });

    /* TODO LTO registration */
    #else
            using PipelineElement = typename PassBuilder::PipelineElement;
            PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM,
                                                  ArrayRef<PipelineElement>) {
                if (Name == "AFLCoverage") {
                    MPM.addPass(AFLCoverage());
                    return true;
                }
                else {
                    return false;
                }
            });

    #endif
        }
    };
}

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

/// @brief Parse json from a file and convert it to an STL unordered map
/// @param distFile
/// @param targetCount
/// @param distMap
/// @return true if success
bool parseDistMapFromJsonFile(
    const std::string distFile, size_t &targetCount,
    std::unordered_map<std::string, std::unordered_map<unsigned, std::vector<int32_t>>> &distMap
)
{
    Json::Value root;
    std::ifstream ifs(distFile, std::ios::in);
    if (!ifs.is_open()) {
        AbortOnError(false, "Failed to open distance file " + distFile);
        return false;
    }
    if (!parseJsonValueFromFile(ifs, root)) {
        AbortOnError(false, "Failed to parse json file " + distFile);
        return false;
    }
    ifs.close();

    // Get the BB distances
    Json::Value::Members members = root.getMemberNames();
    for (auto iter = members.begin(); iter != members.end(); ++iter) {
        std::string filename = *iter;

        if (root[filename].type() != Json::objectValue) {
            AbortOnError(
                false, "The json file '" + distFile + "' was destroyed. The key '" + filename +
                           "' is wrong."
            );
            return false;
        }
        if (distMap.find(filename) == distMap.end()) {
            distMap[filename] = std::unordered_map<unsigned, std::vector<int32_t>>();
        }

        Json::Value::Members innerMembers = root[filename].getMemberNames();
        for (auto i_iter = innerMembers.begin(); i_iter != innerMembers.end(); ++i_iter) {
            std::string lineStr = *i_iter;

            if (root[filename][lineStr].type() != Json::arrayValue) {
                AbortOnError(
                    false, "The json file '" + distFile + "' was destroyed. The key '" +
                               lineStr + "' in key '" + filename + "' is wrong."
                );
                return false;
            }

            if (targetCount == 0) targetCount = root[filename][lineStr].size();
            else if (targetCount != root[filename][lineStr].size()) {
                AbortOnError(
                    false,
                    "The json file '" + distFile +
                        "' was destroyed. The distance array under the key '" + lineStr +
                        "' under the key '" + filename +
                        "' is not compatible with the previous arrays whose size is equal to " +
                        std::to_string(targetCount)
                );
                return false;
            }

            unsigned line = std::stoul(lineStr);
            if (distMap[filename].find(line) == distMap[filename].end()) {
                distMap[filename][line] = std::vector<int32_t>(targetCount, -1);
            }

            for (Json::Value::ArrayIndex i = 0; i < targetCount; i++) {
                if (root[filename][lineStr][i].type() != Json::intValue) {
                    AbortOnError(
                        false, "The json file '" + distFile + "' was destroyed. The " +
                                   std::to_string(i) + "th value under the key '" + lineStr +
                                   "' under the key '" + filename + "' is not an integer"
                    );
                    return false;
                }
                int jsonIndexValue = root[filename][lineStr][i].asInt();
                if (jsonIndexValue >= 0 && (distMap[filename][line][i] < 0 ||
                                            distMap[filename][line][i] > jsonIndexValue))
                    distMap[filename][line][i] = jsonIndexValue;
            }
        }
    }

    return true;
}

} // namespace FGo

PreservedAnalyses FGoModulePass::run(Module &M, ModuleAnalysisManager &MAM)
{
    using namespace FGo;

    /* use new pass manager */
    auto PA = PreservedAnalyses::all();

    // Preprocessing Mode
    if (finalDistanceDir.empty() && !getenv(DIST_DIR_ENVAR)) {
        if (isatty(2) && !getenv("AFL_QUIET")) {
            FGo::HighlightSome(COMPILER_HINT, "(Preprocessing Mode - bitcode)");
        }
        return PA;
    }
    else {
        if (isatty(2) && !getenv("AFL_QUIET")) {
            if (getenv("AFL_USE_ASAN"))
                FGo::HighlightSome(COMPILER_HINT, "(Instrumentation | ASan)");
            else FGo::HighlightSome(COMPILER_HINT, "(Instrumentation | Non-Asan)");
        }
    }

    SmallString<PATH_MAX> basePath;
    if (finalDistanceDir.empty()) {
        finalDistanceDir = getenv(DIST_DIR_ENVAR);
    }

    AbortOnError(
        sys::fs::exists(finalDistanceDir) && sys::fs::is_directory(finalDistanceDir),
        "The path '" + finalDistanceDir + "' doesn't exist or doesn't point to a directory"
    );

    if (projRootDir.empty()) {
        const char *tmpProjRootDir = getenv(PROJ_ROOT_ENVAR);
        AbortOnError(
            tmpProjRootDir,
            "Failed to find the root directory of the project from environment variable"
        );
        projRootDir = std::string(tmpProjRootDir);
    }

    AbortOnError(
        sys::fs::exists(projRootDir) && sys::fs::is_directory(projRootDir),
        "The path '" + projRootDir + "' doesn't exist or doesn't point to a directory"
    );

    basePath.clear();
    sys::fs::real_path(projRootDir, basePath);
    projRootDir = basePath.str().str();
    AbortOnError(
        !projRootDir.empty(),
        "Unexpected root: failed to get the real path of the root directory of the project"
    );

    // Search depth-dirst distance file and backtrace distance file
    basePath = finalDistanceDir;
    sys::path::append(basePath, std::string(DF_DISTANCE_FILENAME) + ".json");
    std::string dfDistanceFile = basePath.str().str();
    AbortOnError(
        sys::fs::exists(dfDistanceFile) && sys::fs::is_regular_file(dfDistanceFile),
        "The distance file '" + dfDistanceFile + "' doesn't exist"
    );

    basePath = finalDistanceDir;
    sys::path::append(basePath, std::string(BT_DISTANCE_FILENAME) + ".json");
    std::string btDistanceFile = basePath.str().str();
    AbortOnError(
        sys::fs::exists(btDistanceFile) && sys::fs::is_regular_file(btDistanceFile),
        "The distance file '" + btDistanceFile + "' doesn't exist"
    );

    // Load json value from files
    std::unordered_map<std::string, std::unordered_map<unsigned, std::vector<int32_t>>>
        dfBBDistMap;
    std::unordered_map<std::string, std::unordered_map<unsigned, std::vector<int32_t>>>
        btBBDistMap;
    size_t targetCount = 0;
    {
        // Get the depth-first distances for BB
        AbortOnError(
            parseDistMapFromJsonFile(dfDistanceFile, targetCount, dfBBDistMap),
            "Failed to parse distance file " + dfDistanceFile
        );
        AbortOnError(
            !dfBBDistMap.empty(),
            "Failed to find any distance for basic blocks in distance file " + dfDistanceFile
        );

        // Get the backtrace distances for BB
        AbortOnError(
            parseDistMapFromJsonFile(btDistanceFile, targetCount, btBBDistMap),
            "Failed to parse distance file " + btDistanceFile
        );
    }

    AbortOnError(targetCount > 0, "The target count is zero");
    AbortOnError(
        targetCount <= FGO_TARGET_MAX_COUNT,
        "The target count is greater that 'FGO_TARGET_MAX_COUNT'=" +
            std::to_string(FGO_TARGET_MAX_COUNT)
    );

    // =======================
    // Instrument distances

    size_t instrBBCount = 0;

    LLVMContext &C = M.getContext();
    IntegerType *Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType *Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType *Int64Ty = IntegerType::getInt64Ty(C);

    #ifdef __x86_64__
    IntegerType *LargestType = Int64Ty;
    #else
    IntegerType *LargestType = Int32Ty;
    #endif

    // [Count for DF] | [DF Dist] | [Count for BT] | [BT Dist]  | [Minimal Dist]
    // 0------------7 | 8------15 | 16----------23 | 24------31 | 32----------39 (byte)

    std::vector<ConstantInt *> dfMapCntLocations(targetCount, nullptr);
    std::vector<ConstantInt *> dfMapDistLocations(targetCount, nullptr);
    std::vector<ConstantInt *> btMapCntLocations(targetCount, nullptr);
    std::vector<ConstantInt *> btMapDistLocations(targetCount, nullptr);
    std::vector<ConstantInt *> minMapDistLocations(targetCount, nullptr);
    for (size_t i = 0; i < targetCount; ++i) {
        dfMapCntLocations[i] = ConstantInt::get(LargestType, MAP_SIZE + i * 40);
        dfMapDistLocations[i] = ConstantInt::get(LargestType, MAP_SIZE + i * 40 + 8);
        btMapCntLocations[i] = ConstantInt::get(LargestType, MAP_SIZE + i * 40 + 16);
        btMapDistLocations[i] = ConstantInt::get(LargestType, MAP_SIZE + i * 40 + 24);
        minMapDistLocations[i] = ConstantInt::get(LargestType, MAP_SIZE + i * 40 + 32);
    }
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

            std::vector<int32_t> dfDistance(targetCount, -1);
            std::vector<int32_t> btDistance(targetCount, -1);
            bool findBBDist = false;
            std::string bbName = "";

            // Get the location of this basic block and fetch the distance
            for (auto &I : BB) {
                if (bbName.empty()) {
                    std::string filePath, fileName;
                    unsigned line = 0;

                    getDebugLocWithPath(&I, filePath, fileName, line, projRootDir);

                    if (filePath.empty() || line == 0) continue;
                    else bbName = filePath + ":" + std::to_string(line);

                    // Depth-first distance
                    if (dfBBDistMap.find(filePath) != dfBBDistMap.end()) {
                        if (dfBBDistMap[filePath].find(line) != dfBBDistMap[filePath].end()) {
                            dfDistance = dfBBDistMap[filePath][line];
                            findBBDist = true;
                        }
                    }
                    else if (dfBBDistMap.find(fileName) != dfBBDistMap.end()) {
                        if (dfBBDistMap[fileName].find(line) != dfBBDistMap[fileName].end()) {
                            dfDistance = dfBBDistMap[fileName][line];
                            findBBDist = true;
                        }
                    }

                    // Backtrace distance
                    if (btBBDistMap.find(filePath) != btBBDistMap.end()) {
                        if (btBBDistMap[filePath].find(line) != btBBDistMap[filePath].end()) {
                            btDistance = btBBDistMap[filePath][line];
                            findBBDist = true;
                        }
                    }
                    else if (btBBDistMap.find(fileName) != btBBDistMap.end()) {
                        if (btBBDistMap[fileName].find(line) != btBBDistMap[fileName].end()) {
                            btDistance = btBBDistMap[fileName][line];
                            findBBDist = true;
                        }
                    }
                }
            }

            BasicBlock::iterator IP = BB.getFirstInsertionPt();
            // if (IP == BB.end()) continue;
            IRBuilder<> IRB(&(*IP));

            // Current location
            unsigned int cur_loc = AFL_R(MAP_SIZE);
            ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);

            // Load previous location
            LoadInst *PrevLoc = IRB.CreateLoad(IRB.getInt32Ty(), AFLPrevLoc);
            PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

            // Load SHM pointer
            LoadInst *MapPtr = IRB.CreateLoad(PointerType::get(Int8Ty, 0), AFLMapPtr);
            MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *MapPtrIdx =
                IRB.CreateGEP(Int8Ty, MapPtr, IRB.CreateXor(PrevLocCasted, CurLoc));

            // Update bitmap
            LoadInst *Counter = IRB.CreateLoad(IRB.getInt8Ty(), MapPtrIdx);
            Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
            IRB.CreateStore(Incr, MapPtrIdx)
                ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            // Set `prev_loc` to `cur_loc >> 1`
            StoreInst *Store =
                IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
            Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            {
                // Increase count at shm
                auto incrCountAtSHM = [&M, &C, &IRB, &MapPtr, &Int8Ty, &LargestType,
                                       &One](ConstantInt *cntLoc) {
                    // Load value from (`MapPtr`+`distLoc`)
                    Value *MapCntPtr = IRB.CreateBitCast(
                        IRB.CreateGEP(Int8Ty, MapPtr, cntLoc), LargestType->getPointerTo()
                    );
                    LoadInst *MapCnt = IRB.CreateLoad(LargestType, MapCntPtr);
                    MapCnt->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                    // Add 1
                    Value *IncrCnt = IRB.CreateAdd(MapCnt, One);

                    // Store
                    IRB.CreateStore(IncrCnt, MapCntPtr)
                        ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
                };

                // Add distance to shm
                auto addDistanceToSHM = [&M, &C, &IRB, &MapPtr, &Int8Ty, &LargestType](
                                            ConstantInt *distLoc, unsigned distance
                                        ) {
                    ConstantInt *distanceValue = ConstantInt::get(LargestType, distance);

                    // Load value from (`MapPtr`+`distLoc`)
                    Value *MapDistPtr = IRB.CreateBitCast(
                        IRB.CreateGEP(Int8Ty, MapPtr, distLoc), LargestType->getPointerTo()
                    );
                    LoadInst *MapDist = IRB.CreateLoad(LargestType, MapDistPtr);
                    MapDist->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                    // Add distance
                    Value *IncrDist = IRB.CreateAdd(MapDist, distanceValue);

                    // Store
                    IRB.CreateStore(IncrDist, MapDistPtr)
                        ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
                };

                // Update minimal distance
                auto updateMinimalDist = [&M, &C, &IRB, &MapPtr, &Int8Ty, &LargestType](
                                             ConstantInt *distLoc, unsigned distance
                                         ) {
                    ConstantInt *distanceValue = ConstantInt::get(LargestType, distance);

                    // Load value from (`MapPtr`+`distLoc`)
                    Value *MapDistPtr = IRB.CreateBitCast(
                        IRB.CreateGEP(Int8Ty, MapPtr, distLoc), LargestType->getPointerTo()
                    );
                    LoadInst *MapDist = IRB.CreateLoad(LargestType, MapDistPtr);
                    MapDist->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

                    // Select the lesser one
                    Value *IsLesser = IRB.CreateICmpSLT(MapDist, distanceValue);
                    Value *UpdatedValue = IRB.CreateSelect(IsLesser, MapDist, distanceValue);

                    // Store
                    IRB.CreateStore(UpdatedValue, MapDistPtr)
                        ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
                };

                for (size_t i = 0; i < targetCount; ++i) {
                    if (dfDistance[i] >= 0) {
                        incrCountAtSHM(dfMapCntLocations[i]);
                        addDistanceToSHM(dfMapDistLocations[i], dfDistance[i]);
                        updateMinimalDist(minMapDistLocations[i], dfDistance[i]);
                    }
                    else if (btDistance[i] >= 0) {
                        incrCountAtSHM(btMapCntLocations[i]);
                        addDistanceToSHM(btMapDistLocations[i], btDistance[i]);
                    }
                }
            }

            if (findBBDist) ++instrBBCount;
        }
    }

    // Some hints
    if (isatty(2) && !getenv("AFL_QUIET")) {
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

    return PA;
}

#endif