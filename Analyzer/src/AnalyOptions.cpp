/**
 *
 *
 */

#include "AnalyOptions.h"

#include "SVF-LLVM/LLVMUtil.h"
#include "Util/ExtAPI.h"

#include <cstring>
#include <iostream>

namespace FGo
{
namespace Analy
{
// static void abortOnError(bool _flag, const std::string &_msg = "Some errors occurred")
// {
//     if (!_flag) {
//         std::cerr << _msg << std::endl;
//         exit(1);
//     }
// }

void Options::printUsage(const String &binaryName)
{
    std::cout
        << "Analyze LLVM bitcode file via SVF and calculate distances for function calls, "
           "blocks and basic blocks in ICFG.\nOnly output final distances for basic blocks by "
           "default.\n\n"
        << "Usage: " << binaryName
        << " -b BITCODE_FILE [BITCODE_FILE1...] | BINARY_FILE -t TARGET_FILE [-o OUPUT_DIR] "
           "[-r PROJ_ROOT_DIR] [-e EXT_DIR] [--svf] [--cg] [--icfg] [--calldist] "
           "[--blockpredist] [--blockdist] [--bbdist] [--nonfinal] \n\n"
        << "Options:\n"
        << "  -b, --bitcode   The bitcode file(s) or the program binary file\n"
        << "  -t, --target    The file containing targets\n"
        << "  -o, --ouput     The output directory, the default is the current working "
           "directory\n"
        << "  -r, --rootdir   The root directory of the project, the default is from env '"
        << PROJ_ROOT_DIR_ENV << "'\n"
        << "  -e, --extdir    The directory containing extension files (extapi.bc), the "
           "default is the executable directory\n"
        << "  --svf           Dump the SVF analysis statistics\n"
        << "  --cg            Dump the call graphs\n"
        << "  --icfg          Dump the ICFGs\n"
        << "  --calldist      Dump the distances for function calls in ICFG\n"
        << "  --blockpredist  Dump the pre-completion distances for blocks in ICFG\n"
        << "  --blockdist     Dump the distances for blocks in ICFG\n"
        << "  --bbdist        Dump the distances for basic blocks\n"
        << "  --nonfinal      Never dump the final distances for basic blocks" << std::endl;
}

void Options::parseArguments(int argc, char **argv)
{
    FGo::AbortOnError(argc > 1, "No arguments found; use '-h' or '--help' to check the usage");

    int arg_num = argc;
    char **arg_value = argv;

    m_moduleNames.clear();
    m_outDirectory = "";
    m_projRootDir = "";
    m_extDirectory = "";
    m_targetFile = "";
    m_isDumpSVFStats = false;
    m_isDumpCG = false;
    m_isDumpICFG = false;
    m_isDumpCallDist = false;
    m_isDumpBlockPreDist = false;
    m_isDumpBlockDist = false;
    m_isDumpBBDist = false;
    m_isDumpFinalDist = true;

    int index = 1;
    while (index < arg_num) {
        if (strncmp(arg_value[index], "-h", 2) == 0 ||
            strncmp(arg_value[index], "--help", 6) == 0)
        {
            FGo::AbortOnError(argc == 2, "Redundant arguments along with the helper option");
            printUsage(arg_value[0]);
            exit(0);
        }
        else if (strncmp(arg_value[index], "-b", 2) == 0 || strncmp(arg_value[index], "--bitcode", 9) == 0)
        {
            ++index;
            FGo::AbortOnError(index < arg_num, "No specified bitcode file or binary file");
            FGo::AbortOnError(
                pathExists(arg_value[index]) && pathIsFile(arg_value[index]),
                String("The specified path '") + arg_value[index] + "' doesn't point to a file"
            );
            if (SVF::LLVMUtil::isIRFile(arg_value[index])) {
                m_moduleNames.push_back(arg_value[index++]);
                while (index < arg_num && arg_value[index][0] != '-') {
                    FGo::AbortOnError(
                        pathExists(arg_value[index]) && pathIsFile(arg_value[index]),
                        String("The specified path '") + arg_value[index] +
                            "' doesn't point to a file"
                    );
                    FGo::AbortOnError(
                        SVF::LLVMUtil::isIRFile(arg_value[index]),
                        String("The file '") + arg_value[index] + "' is not a bitcode file"
                    );
                    m_moduleNames.push_back(arg_value[index++]);
                }
                --index;
            }
            else {
                String fileDir, fileName;
                getFileNameAndDirectory(arg_value[index], fileName, fileDir);
                FGo::AbortOnError(
                    !fileDir.empty() && !fileName.empty(),
                    String("Failed to parse the file name and parent directory of ") +
                        arg_value[index]
                );
                StringVector bcFiles;
                getMatchedFiles(fileDir, fileName + ".0.0.*.bc", bcFiles);
                FGo::AbortOnError(
                    !bcFiles.empty(),
                    String("Failed to find relevant bitcode files under ") + fileDir
                );
                for (const auto &bcFile : bcFiles)
                    FGo::AbortOnError(
                        SVF::LLVMUtil::isIRFile(bcFile),
                        String("The file '") + bcFile + "' is not bitcode file"
                    );
                m_moduleNames = bcFiles;
            }
            FGo::AbortOnError(!m_moduleNames.empty(), "No specified bitcode file");
        }
        else if (strncmp(arg_value[index], "-o", 2) == 0 || strncmp(arg_value[index], "--output", 8) == 0)
        {
            ++index;
            FGo::AbortOnError(index < arg_num, "No specified output directory");
            FGo::AbortOnError(
                pathExists(arg_value[index]) && pathIsDirectory(arg_value[index]),
                String("The specified path '") + arg_value[index] +
                    "' doesn't point to a directory"
            );
            m_outDirectory = arg_value[index];
        }
        else if (strncmp(arg_value[index], "-t", 2) == 0 || strncmp(arg_value[index], "--target", 8) == 0)
        {
            ++index;
            FGo::AbortOnError(index < arg_num, "No specified target file");
            FGo::AbortOnError(
                pathExists(arg_value[index]) && pathIsFile(arg_value[index]),
                String("The specified path '") + arg_value[index] + "' doesn't point to a file"
            );
            m_targetFile = arg_value[index];
        }
        else if (strncmp(arg_value[index], "-r", 2) == 0 || strncmp(arg_value[index], "--rootdir", 9) == 0)
        {
            ++index;
            FGo::AbortOnError(index < arg_num, "No specified project root directory");
            FGo::AbortOnError(
                pathExists(arg_value[index]) && pathIsDirectory(arg_value[index]),
                String("The specified path '") + arg_value[index] +
                    "' doesn't point to a directory"
            );
            m_projRootDir = arg_value[index];
        }
        else if (strncmp(arg_value[index], "-e", 2) == 0 || strncmp(arg_value[index], "--extdir", 8) == 0)
        {
            ++index;
            FGo::AbortOnError(index < arg_num, "No specified output directory");
            FGo::AbortOnError(
                pathExists(arg_value[index]) && pathIsDirectory(arg_value[index]),
                String("The specified path '") + arg_value[index] +
                    "' doesn't point to a directory"
            );
            m_extDirectory = arg_value[index];
        }
        else if (strncmp(arg_value[index], "--svf", 5) == 0) {
            m_isDumpSVFStats = true;
        }
        else if (strncmp(arg_value[index], "--cg", 4) == 0) {
            m_isDumpCG = true;
        }
        else if (strncmp(arg_value[index], "--icfg", 6) == 0) {
            m_isDumpICFG = true;
        }
        else if (strncmp(arg_value[index], "--calldist", 10) == 0) {
            m_isDumpCallDist = true;
        }
        else if (strncmp(arg_value[index], "--blockpredist", 14) == 0) {
            m_isDumpBlockPreDist = true;
        }
        else if (strncmp(arg_value[index], "--blockdist", 11) == 0) {
            m_isDumpBlockDist = true;
        }
        else if (strncmp(arg_value[index], "--bbdist", 8) == 0) {
            m_isDumpBBDist = true;
        }
        else if (strncmp(arg_value[index], "--nonfinal", 10) == 0) {
            m_isDumpFinalDist = false;
        }
        else {
            FGo::AbortOnError(
                false, "Unknown argument option '" + std::string(arg_value[index]) + "'"
            );
        }
        ++index;
    }

    if (m_outDirectory.empty()) m_outDirectory = getCurrentPath();

    m_rawCGFile = joinPath(m_outDirectory, RAW_CG_NAME);
    m_optCGFile = joinPath(m_outDirectory, OPT_CG_NAME);
    m_rawICFGFile = joinPath(m_outDirectory, RAW_ICFG_NAME);
    m_optICFGFile = joinPath(m_outDirectory, OPT_ICFG_NAME);
    m_callDistFile = joinPath(m_outDirectory, CALL_DIST_NAME);
    m_blockPreDistFile = joinPath(m_outDirectory, PRE_BLOCK_DIST_NAME);
    m_blockFinalDistFile = joinPath(m_outDirectory, DF_BLOCK_DIST_NAME);
    m_blockPseudoDistFile = joinPath(m_outDirectory, BT_BLOCK_DIST_NAME);
    m_bbDFDistFile = joinPath(m_outDirectory, DF_BB_DIST_NAME);
    m_bbBTDistFile = joinPath(m_outDirectory, BT_BB_DIST_NAME);
    m_bbFinalDistFile = joinPath(m_outDirectory, FINAL_BB_DIST_NAME);

    // Check project root directory
    if (m_projRootDir.empty()) {
        char *dirFromEnv = getenv(PROJ_ROOT_DIR_ENV.c_str());
        if (dirFromEnv) {
            FGo::AbortOnError(
                pathExists(dirFromEnv) && pathIsDirectory(dirFromEnv),
                String("the specified path doesn't point to a directory: ") + dirFromEnv
            );
            m_projRootDir = dirFromEnv;
        }
    }
    FGo::AbortOnError(
        !m_projRootDir.empty(), "Failed to find the root directory of the project. Please "
                                "specify it via argument or environment variable."
    );
    if (m_projRootDir.back() == '/') m_projRootDir.pop_back();

    // Check ext file
    std::string extAPIFile("");
    if (!m_extDirectory.empty()) {
        extAPIFile = joinPath(m_extDirectory, EXT_API_FILENAME);
    }
    else {
        auto exeDirPath = getExeDirPath();
        FGo::AbortOnError(
            !exeDirPath.empty(), "Failed to get the parent directory of current executable"
        );
        extAPIFile = joinPath(exeDirPath, EXT_API_FILENAME);
    }
    FGo::AbortOnError(
        pathExists(extAPIFile) && pathIsFile(extAPIFile),
        "Failed to find the " + EXT_API_FILENAME + " under " + extAPIFile
    );
    SVF::ExtAPI::setExtBcPath(extAPIFile);

    // Check target file
    if (m_targetFile.empty()) {
        if (m_isDumpCallDist || m_isDumpBlockPreDist || m_isDumpBlockDist || m_isDumpBBDist ||
            m_isDumpFinalDist)
        {
            FGo::AbortOnError(false, "No target file specified");
        }
    }
    else {
        FGo::AbortOnError(
            pathExists(m_targetFile) && pathIsFile(m_targetFile),
            "The target file '" + m_targetFile + "' doesn't exist"
        );
    }

    // Check utility
    FGo::AbortOnError(
        m_isDumpSVFStats || m_isDumpCG || m_isDumpICFG || m_isDumpCallDist ||
            m_isDumpBlockPreDist || m_isDumpBlockDist || m_isDumpBBDist || m_isDumpFinalDist,
        "Nothing to do!"
    );
}
} // namespace Analy
} // namespace FGo