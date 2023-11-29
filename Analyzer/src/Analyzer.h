/**
 * ====== Analyzer.h ============================
 *
 * Copyright (C) 2023 Joshua Yao
 *
 * Nov. 16 2023
 */

#ifndef JY_ANALYZER_H_
#define JY_ANALYZER_H_

#include "AnalyUtils.h"
#include "Graphs/SVFG.h"
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFModule.h"
#include "WPA/Andersen.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace FGo
{

namespace Analy
{

class ElementCountMap
{
private:
    Map<String, unsigned> m_elements;

public:
    typedef Map<String, unsigned>::iterator EleMapIter;

public:
    ElementCountMap()
    {}
    ~ElementCountMap() = default;

    bool hasElement(const String &_ele)
    {
        return m_elements.find(_ele) != m_elements.end();
    }

    unsigned getElementCount(const String &_ele)
    {
        return hasElement(_ele) ? m_elements[_ele] : 0;
    }

    void pushElement(const String &_ele)
    {
        m_elements[_ele]++;
    }

    void popElement(const String &_ele)
    {
        auto count = getElementCount(_ele);
        if (count == 1) m_elements.erase(_ele);
        else if (count > 1) m_elements[_ele]--;
    }

    inline EleMapIter begin()
    {
        return m_elements.begin();
    }

    inline EleMapIter end()
    {
        return m_elements.end();
    }
};

class SVFAnalyzer
{
private:
    SVF::SVFModule *m_svfModule;
    SVF::SVFIR *m_pag;
    SVF::Andersen *m_ander;
    SVF::PTACallGraph *m_ptaCallGraph;
    SVF::ICFG *m_ICFG;

    String m_statsInfo;

    ProgressBar m_pBar;

public:
    SVFAnalyzer() :
        m_svfModule(nullptr), m_pag(nullptr), m_ander(nullptr), m_ptaCallGraph(nullptr),
        m_ICFG(nullptr)
    {}

    /// @brief Analyze via pointer analysis
    /// @param moduleNames
    void analyze(const StringVector &moduleNames);

    /// @brief Release LLVM and SVF resources
    void release();

    /// @brief Get SVF statistics
    /// @return
    String getStats();

    /// @brief Get PTA call graph
    /// @return
    SVF::PTACallGraph *getPTACallGraph();

    /// @brief Get raw ICFG
    /// @return
    SVF::ICFG *getICFG();
};

class GraphAnalyzer
{
private:
    /// @brief A class representing the location of an ICFG node
    struct NodeLocation
    {
        StringVector filePathChunks;

        String file;
        unsigned line;
        unsigned column;

        NodeLocation() : line(0), column(0), file("")
        {}

        /// @brief Constructor with location information
        /// @param _line
        /// @param _column
        /// @param _filePath
        NodeLocation(unsigned _line, unsigned _column, const String &_filePath);

        /// @brief Constructor with source location string from SVF IR
        /// @param sourceLoc
        /// @exception `AnalyException`
        NodeLocation(const String &sourceLoc);
    };

    /// @brief A class representing the location of a target node
    struct TargetLocation
    {
        StringVector filePathChunks;
        unsigned line;

        double weight;

        TargetLocation() : line(0), weight(1.0)
        {}

        /// @brief Constructor with line number and file path
        /// @param _line
        /// @param _filePath
        /// @param _weight default is 1.0
        TargetLocation(unsigned _line, const String &_filePath, double _weight = 1.0);

        /// @brief Constructor with a string like "main.c:233"
        /// @param targetLoc
        TargetLocation(const String &targetLoc);

        /// @brief Compare with a location including line number and file path
        /// @param _line
        /// @param _filePath
        /// @return
        bool isTarget(unsigned _line, const String &_filePath);

        /// @brief Compare with a location string from SVF IR
        /// @param sourceLoc
        /// @return
        bool isTarget(const String &sourceLoc);

        /// @brief Compare with a node location
        /// @param nodeLoc
        /// @return
        bool isTarget(const NodeLocation &nodeLoc);

        /// @brief Compare with a node location. Only when the
        /// node location is not empty and the location is not
        /// equal to the target location, this function returns true.
        /// @param nodeLoc
        /// @return
        bool isNotTarget(const NodeLocation &nodeLoc);
    };

private:
    SVF::PTACallGraph *m_callgraph;
    SVF::ICFG *m_icfg;

    bool m_cg_processed;
    bool m_icfg_analyzed;
    bool m_icfg_processed;

    /// @brief A constant distance for an external function call
    const int32_t EXTERN_CALL_DIST = 30;

    /// @brief A constant distance for a recursive function call
    const int32_t RECURSIVE_CALL_DIST = 25;

    /// @brief A constant distance for a block inner a function call
    const int32_t INNER_CALL_DIST = 20;

    Map<String, ElementCountMap> m_callMap;
    Map<String, ElementCountMap> m_indCallMap;

    /// @brief Node locations
    Map<SVF::NodeID, NodeLocation> m_nodeLocations;

    /// @brief Target count
    size_t m_targetCount;

    /// @brief Target locations
    Vector<TargetLocation> m_targetLocations;

    /// @brief Whether the targets were loaded
    bool m_isTargetsLoaded;

    /// @brief Target ICFG nodes
    Vector<Set<SVF::NodeID>> m_targetNodes;

    /// @brief A map from function calls to distances
    Map<String, Pair<uint32_t, Vector<int32_t>>> m_callDistMap;

    /// @brief Whether the distances for function calls were calculated
    bool m_isCallDistCalc;

    /// @brief A map from blocks to distances
    Map<SVF::NodeID, Vector<int32_t>> m_blockDistMap;

    /// @brief Whether the distances for blocks were calculated
    bool m_isBlockDistCalc;

    Mutex m_blockDistMutex;

    /// @brief A map from blocks to pseudo-distances
    Map<SVF::NodeID, Vector<int32_t>> m_blockPseudoDistMap;

    /// @brief Whether the pseudo-distances fro blocks were calculated
    bool m_isPseudoDistCalc;

    Mutex m_blockPseudoDistMutex;

    /// @brief A simple call graph
    Map<const SVF::FunEntryICFGNode *, Set<const SVF::FunEntryICFGNode *>> m_simpleCallGraph;

    /// @brief Whether the simple CG was loaded
    bool m_isSimpleCGLoaded;

    /// @brief Dynamic set for function calls
    Set<const SVF::FunEntryICFGNode *> m_dynCallSet;

    ProgressBar m_progressBar;

private:
    void dumpRawPTACallGraph(const String &filename);

    void dumpProcPTACallGraph(const String &filename);

    void updateICFGWithIndirectCalls();

    void dumpRawICFGWithAnalysis(const String &filename);

    void dumpProcICFGWithAnalysis(const String &filename);

    void subCalculateCalls(const SVF::FunEntryICFGNode *funcEntryNode);

    void loadTargets(const String &targetFile);

    void loadSimpleCallGraph();

    Vector<int32_t> singleCalculateBlock(const SVF::ICFGNode *node);

    void dfsCalculateBlocks(
        const SVF::ICFGNode *node, Map<SVF::NodeID, Vector<int32_t>> &distMap,
        Set<SVF::NodeID> &curProcNodes
    );

    void threadCalculateBlocks(const SVF::FunEntryICFGNode *funcEntryNode);

    void subCalculateFinalBlocks(const SVF::FunEntryICFGNode *funcEntryNode);

public:
    GraphAnalyzer() :
        m_callgraph(nullptr), m_icfg(nullptr), m_cg_processed(false), m_icfg_analyzed(false),
        m_icfg_processed(false), m_isTargetsLoaded(false), m_isSimpleCGLoaded(false),
        m_isCallDistCalc(false), m_isBlockDistCalc(false), m_isPseudoDistCalc(false)
    {}
    GraphAnalyzer(SVF::PTACallGraph *_cg, SVF::ICFG *_icfg) :
        m_callgraph(_cg), m_icfg(_icfg), m_cg_processed(false), m_icfg_analyzed(false),
        m_icfg_processed(false), m_isTargetsLoaded(false), m_isSimpleCGLoaded(false),
        m_isCallDistCalc(false), m_isBlockDistCalc(false), m_isPseudoDistCalc(false)
    {}
    GraphAnalyzer(const GraphAnalyzer &_other);
    ~GraphAnalyzer()
    {
        m_callgraph = nullptr;
        m_icfg = nullptr;
    }

    /// @brief Dump call graph to a dot file.
    /// @param filename dot file name without file extension
    /// @param processing whether output the processed call graph
    /// @exception `AnalyException`
    void dumpPTACallGraph(const String &filename, bool processing = true);

    /// @brief Dump raw call graph and processed call graph.
    /// @param rawFileName raw dot file name without file extension
    /// @param procFileName processed dot file name without file extension
    /// @exception `AnalyException`
    void dumpPTACallGraph(const String &rawFileName, const String &procFileName);

    /// @brief Dump analyzed ICFG to a dot file.
    /// @param filename dot file name without file extension
    /// @param processing whether output the processed ICFG
    /// @exception `AnalyException`
    void dumpICFGWithAnalysis(const String &filename, bool processing = true);

    /// @brief Dump analyzed ICFG and processed analyzed ICFG.
    /// @param rawFileName raw dot file name without file extension
    /// @param procFileName processed dot file name without file extension
    /// @exception `AnalyException`
    void dumpICFGWithAnalysis(const String &rawFileName, const String &procFileName);

    /// @brief Calculate distances for function calls in ICFG.
    /// @param targetFile a file containing target locations
    /// @exception `AnalyException`
    void calculateCallsInICFG(const String &targetFile);

    /// @brief Dump the target nodes and distances for function calls
    /// @param outCallsDistFile
    /// @exception `AnalyException`
    /// @exception `std::exception`
    void dumpCallsDistance(const String &outCallsDistFile);

    /// @brief Calculate pre-completion distances for blocks in ICFG.
    /// @exception `AnalyException`
    /// @exception `std::exception`
    void calculateBlocksPreDistInICFG();

    /// @brief Calculate final distances for blocks in ICFG.
    /// @exception `AnalyException`
    /// @exception `std::exception`
    void calculateBlocksFinalDistInICFG();

    /// @brief Dump the distances for blocks.
    /// @param outBlocksDistFile
    /// @param isPseudo
    /// @exception `AnalyException`
    /// @exception `std::exception`
    void dumpBlocksDistance(
        const String &outBlocksDistFile, const String &projRootDir, bool isPseudo = false
    );

    /// @brief Dump the distances for basic blocks
    /// @param outBBDistFile
    /// @param projRootDir
    /// @param isPseudo
    /// @exception `AnalyException`
    /// @exception `std::exception`
    void dumpBasicBlockDistance(
        const String &outBBDistFile, const String &projRootDir, bool isPseudo = false
    );

    /// @brief Dump the final distances for basic blocks
    /// @param outBBDistanceFile
    /// @param projRootDir
    void dumpBasicBlockFinalDistance(
        const String &outBBDistanceFile, const String &projRootDir
    );
};

} // namespace Analy

} // namespace FGo

#endif