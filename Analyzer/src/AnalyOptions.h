/**
 *
 *
 */

#ifndef JY_ANALYOPTIONS_H_
#define JY_ANALYOPTIONS_H_

#include "AnalyUtils.h"
#include "FGoDefs.h"

namespace FGo
{
namespace Analy
{
class Options
{

private:
    const String RAW_CG_NAME = "callgraph.raw";
    const String OPT_CG_NAME = "callgraph.opt";
    const String RAW_ICFG_NAME = "icfg.raw";
    const String OPT_ICFG_NAME = "icfg.opt";
    const String CALL_DIST_NAME = "calls.distance";
    const String PRE_BLOCK_DIST_NAME = "blocks.distance.pre";
    const String DF_BLOCK_DIST_NAME = "blocks.distance.df";
    const String BT_BLOCK_DIST_NAME = "blocks.distance.bt";
    const String DF_BB_DIST_NAME = DF_DISTANCE_FILENAME;
    const String BT_BB_DIST_NAME = BT_DISTANCE_FILENAME;
    const String FINAL_BB_DIST_NAME = FINAL_DISTANCE_FILENAME;
    const String TARGET_INFO_NAME = TARGET_INFO_FILENAME;
    const String EXT_API_FILENAME = "extapi.bc";

    const String PROJ_ROOT_DIR_ENV = PROJ_ROOT_ENVAR;

    String m_outDirectory;
    String m_extDirectory;

    void printUsage(const String &binaryName);

public:
    StringVector m_moduleNames; // Bitcode file vector
    String m_targetFile;        // File containing target locations

    String m_rawCGFile;           // Raw CG dot file name
    String m_rawICFGFile;         // Raw ICFG dot file name
    String m_optCGFile;           // Optimized CG dot file name
    String m_optICFGFile;         // Optimized ICFG dot file name
    String m_callDistFile;        // File containing distances for function calls
    String m_blockPreDistFile;    // File conatining pre-completion distances for blocks in ICFG
    String m_blockFinalDistFile;  // File containing final distances for blocks in ICFG
    String m_blockPseudoDistFile; // File conatining pseudo distances for blocks in ICFG
    String m_bbDFDistFile;        // File containing depth-first distances for basic blocks
    String m_bbBTDistFile;        // File conatining backtrace distances for basic blocks
    String m_bbFinalDistFile;     // File containing final distances for basic blocks

    String m_targetFuzzingInfoFile; // File containing target information for fuzzing

    String m_projRootDir; // Root directory of the relevant project

    bool m_isDumpSVFStats;     // Whether dump SVF statistics
    bool m_isDumpCG;           // Whether dump CG
    bool m_isDumpICFG;         // Whether dump ICFG
    bool m_isDumpCallDist;     // Whether dump call distances
    bool m_isDumpBlockPreDist; // Whether dump pre-completion distances for blocks
    bool m_isDumpBlockDist;    // Whether dump distances for blocks in ICFG
    bool m_isDumpBBDist;       // Whether dump distances for basic blocks
    bool m_isUsingDistrib;     // Whether use the estimation of probabilistic distribution

    Options();

    /// @brief Parse arguments from command line
    /// @param argc
    /// @param argv
    void parseArguments(int argc, char **argv);
};

} // namespace Analy
} // namespace FGo

#endif