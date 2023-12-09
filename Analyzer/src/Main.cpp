/**
 *
 *
 */

#include "AnalyOptions.h"
#include "Analyzer.h"
#include "FGoUtils.hpp"

#include <iostream>

using namespace FGo;

int main(int argc, char **argv)
{
    Analy::Options options;
    options.parseArguments(argc, argv);

    try {
        // Analyze via pointer analysis
        Analy::SVFAnalyzer svfAnaly;
        svfAnaly.analyze(options.m_moduleNames);

        if (options.m_isDumpSVFStats) std::cout << svfAnaly.getStats() << std::endl;

        // Check tasks
        if (!options.m_isDumpCG && !options.m_isDumpICFG && !options.m_isDumpCallDist &&
            !options.m_isDumpBlockPreDist && !options.m_isDumpBlockDist &&
            !options.m_isDumpBBDist)
            return 0;

        // Analyze graphs
        Analy::GraphAnalyzer graphAnaly =
            Analy::GraphAnalyzer(svfAnaly.getPTACallGraph(), svfAnaly.getICFG());

        if (options.m_isDumpCG)
            graphAnaly.dumpPTACallGraph(options.m_rawCGFile, options.m_optCGFile);
        if (options.m_isDumpICFG)
            graphAnaly.dumpICFGWithAnalysis(options.m_rawICFGFile, options.m_optICFGFile);

        // Check atsks
        if (!options.m_isDumpCallDist && !options.m_isDumpBlockPreDist &&
            !options.m_isDumpBlockDist && !options.m_isDumpBBDist)
            return 0;

        // Calculate call distances
        graphAnaly.calculateCallsInICFG(options.m_targetFile);

        if (options.m_isDumpCallDist) graphAnaly.dumpCallsDistance(options.m_callDistFile);

        // Check tasks
        if (!options.m_isDumpBlockPreDist && !options.m_isDumpBlockDist &&
            !options.m_isDumpBBDist)
            return 0;

        // Calculate block distances
        graphAnaly.calculateBlocksPreDistInICFG();

        // Dump block distances
        if (options.m_isDumpBlockPreDist)
            graphAnaly.dumpBlocksDistance(options.m_blockPreDistFile, options.m_projRootDir);

        // Check tasks
        if (!options.m_isDumpBlockDist && !options.m_isDumpBBDist) return 0;

        // Calculate final distances for blocks
        graphAnaly.calculateBlocksFinalDistInICFG();

        if (options.m_isDumpBlockDist) {
            graphAnaly.dumpBlocksDistance(options.m_blockFinalDistFile, options.m_projRootDir);
            graphAnaly.dumpBlocksDistance(
                options.m_blockPseudoDistFile, options.m_projRootDir, true
            );
        }

        if (options.m_isDumpBBDist) {
            graphAnaly.dumpBasicBlockDistance(
                options.m_bbDFDistFile, options.m_projRootDir, false
            );
            graphAnaly.dumpBasicBlockDistance(
                options.m_bbBTDistFile, options.m_projRootDir, true
            );
        }

        graphAnaly.dumpFuzzingInfo(options.m_targetFuzzingInfoFile, options.m_isUsingDistrib);

        // Release resources
        svfAnaly.release();
    }
    catch (const Analy::UnexpectedException &e) {
        FGo::AbortOnError(false, Analy::String("Unexpected error: ") + e.what());
    }
    catch (const Analy::InvalidDataSetException &e) {
        FGo::AbortOnError(false, Analy::String("Invalid data set: ") + e.what());
    }
    catch (const Analy::AnalyException &e) {
        FGo::AbortOnError(false, Analy::String("Analysis error: ") + e.what());
    }
    catch (const std::exception &e) {
        FGo::AbortOnError(false, Analy::String("System error: ") + e.what());
    }

    return 0;
}