/**
 * ====== Analyzer.cpp ============================
 *
 * Copyright (C) 2023 Joshua Yao
 *
 * Nov. 16 2023
 */

#include "Analyzer.h"

#include "AnalyStats.h"
#include "AnalyThreadPool.h"
#include "Graphs/SVFG.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/SVFUtil.h"
#include "WPA/Andersen.h"

#include "json/json.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace FGo
{
namespace Analy
{

void SVFAnalyzer::analyze(const StringVector &moduleNames)
{
    uint64_t fileSize = 0;
    for (const auto &moduleName : moduleNames) {
        int64_t tmpSize = getFileSize(moduleName);
        if (tmpSize >= 0) fileSize += tmpSize;
    }
    uint64_t seconds = 300;

    if (fileSize > 0) {
        // Some empirical values from partial experiments
        seconds = fileSize * 60ull * 3ull / (1024ull * 1024ull) / 4ull;
        seconds = seconds == 0ull ? (seconds + 60ull) : seconds;
    }
    m_pBar.start(
        0, "Loading bitcode file(s) and analyzing SVF module via Andersen Algorithm", true
    );
    String timeValue = seconds < 60ull ? (toString(seconds) + " second(s)")
                                       : (toString(seconds / 60ull) + " minute(s)");
    m_pBar.show("It may take about " + timeValue + ".");

    OutputCapture outCapture;
    outCapture.start();

    // Get SVF module
    m_svfModule = SVF::LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNames);

    // Build Program Assignment Graph (SVFIR)
    SVF::SVFIRBuilder builder(m_svfModule);
    m_pag = builder.build();

    // Create Andersen's pointer analysis
    m_ander = SVF::AndersenWaveDiff::createAndersenWaveDiff(m_pag);

    // Get PTA call graph
    m_ptaCallGraph = m_ander->getPTACallGraph();

    // Get ICFG
    m_ICFG = m_ander->getICFG();

    outCapture.stop();
    m_statsInfo = outCapture.getCapturedContent();

    m_pBar.stop();
}

void SVFAnalyzer::release()
{
    // Release resources
    SVF::AndersenWaveDiff::releaseAndersenWaveDiff();
    SVF::SVFIR::releaseSVFIR();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
    llvm::llvm_shutdown();
}

String SVFAnalyzer::getStats()
{
    return m_statsInfo;
}

SVF::PTACallGraph *SVFAnalyzer::getPTACallGraph()
{
    return m_ptaCallGraph;
}

SVF::ICFG *SVFAnalyzer::getICFG()
{
    return m_ICFG;
}

GraphAnalyzer::GraphAnalyzer(const GraphAnalyzer &_other)
{
    m_callgraph = _other.m_callgraph;
    m_icfg = _other.m_icfg;

    m_cg_processed = _other.m_cg_processed;
    m_icfg_analyzed = _other.m_icfg_analyzed;
    m_icfg_processed = _other.m_icfg_processed;

    m_callMap = _other.m_callMap;
    m_indCallMap = _other.m_indCallMap;

    m_nodeLocations = _other.m_nodeLocations;
    m_targetCount = _other.m_targetCount;
    m_targetLocations = _other.m_targetLocations;
    m_isTargetsLoaded = _other.m_isTargetsLoaded;
    m_targetNodes = _other.m_targetNodes;

    m_callDistMap = _other.m_callDistMap;
    m_isCallDistCalc = _other.m_isCallDistCalc;

    m_blockDistMap = _other.m_blockDistMap;
    m_isBlockDistCalc = _other.m_isBlockDistCalc;

    m_blockPseudoDistMap = _other.m_blockPseudoDistMap;
    m_isPseudoDistCalc = _other.m_isPseudoDistCalc;

    m_simpleCallGraph = _other.m_simpleCallGraph;
    m_isSimpleCGLoaded = _other.m_isSimpleCGLoaded;

    m_dynCallSet = _other.m_dynCallSet;

    m_progressBar = _other.m_progressBar;
}

void GraphAnalyzer::dumpRawPTACallGraph(const String &filename)
{
    m_progressBar.start(0, "Writing raw PTA call graph", true);
    m_progressBar.show("Dumping to " + filename + ".dot");

    OutputCapture outCapture;
    outCapture.start();

    m_callgraph->dump(filename);

    outCapture.stop();
    m_progressBar.stop();
}

void GraphAnalyzer::dumpProcPTACallGraph(const String &filename)
{
    if (m_callgraph == nullptr) throw AnalyException("The pointer to call graph is null");

    m_progressBar.start(0, "Writing optimized PTA call graph", true);
    m_progressBar.show("Dumping to " + filename + ".dot");

    // indirect calls
    if (!m_cg_processed) {
        m_indCallMap.clear();
        auto indCallMap = m_callgraph->getIndCallMap();
        for (auto iter = indCallMap.begin(), _iter = indCallMap.end(); iter != _iter; ++iter) {
            auto callerName = iter->first->getFun()->getName();
            if (m_indCallMap.find(callerName) == m_indCallMap.end())
                m_indCallMap[callerName] = ElementCountMap();
            for (auto callee : iter->second) {
                auto calleeName = callee->getName();
                m_indCallMap[callerName].pushElement(calleeName);
            }
        }
    }
    m_callMap = m_indCallMap;

    String filepath = filename + ".dot";
    std::ofstream outFile(filepath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) throw AnalyException("Failed to open the dot file " + filepath);

    outFile << "digraph \"Call Graph\" {" << std::endl;
    outFile << "\tlabel=\"Call Graph\";\n" << std::endl;
    Set<SVF::NodeID> visited;
    Map<String, SVF::NodeID> funcNameIDMap;
    for (auto iter = m_callgraph->begin(), _iter = m_callgraph->end(); iter != _iter; ++iter) {
        auto nodeID = iter->second->getId();
        auto nodeIDStr = getNodeIDString(iter->second->getId());
        auto funcName = iter->second->getFunction()->getName();
        funcNameIDMap[funcName] = nodeID;
        String externFlag =
            SVF::SVFUtil::isExtCall(iter->second->getFunction()) ? "true" : "false";
        if (visited.find(nodeID) == visited.end()) {
            visited.emplace(nodeID);
            outFile << "\t" << nodeIDStr << " ["
                    << "function=\"" << funcName << "\",extern=" << externFlag << "];"
                    << std::endl;
        }
        if (m_callMap.find(funcName) == m_callMap.end()) {
            m_callMap[funcName] = ElementCountMap();
        }
        for (auto t_iter = iter->second->OutEdgeBegin(), _e_iter = iter->second->OutEdgeEnd();
             t_iter != _e_iter; ++t_iter)
        {
            SVF::PTACallGraphEdge *edge = *t_iter;
            SVF::PTACallGraphNode *targetNode = edge->getDstNode();
            auto targetNodeIDStr = getNodeIDString(targetNode->getId());
            auto targetFuncName = targetNode->getFunction()->getName();
            outFile << "\t" << nodeIDStr << " -> " << targetNodeIDStr << " [indirect=false];"
                    << std::endl;
            m_callMap[funcName].pushElement(targetFuncName);
        }
    }
    // add edges from indirect calls
    for (auto &iter : m_indCallMap) {
        auto funcName = iter.first;
        if (funcNameIDMap.find(funcName) == funcNameIDMap.end())
            throw AnalyException(String("Function name \"") + funcName + "\" not found in map");
        auto nodeIDStr = getNodeIDString(funcNameIDMap[funcName]);
        for (auto t_iter = iter.second.begin(); t_iter != iter.second.end(); ++t_iter) {
            auto targetFuncName = t_iter->first;
            if (funcNameIDMap.find(targetFuncName) != funcNameIDMap.end()) {
                auto targetNodeIDStr = getNodeIDString(funcNameIDMap[targetFuncName]);
                outFile << "\t" << nodeIDStr << " -> " << targetNodeIDStr << " [indirect=true];"
                        << std::endl;
            }
            else
                throw AnalyException(
                    String("Function name \"") + targetFuncName + "\" not found in map"
                );
        }
    }
    outFile << "}" << std::endl;
    outFile.close();
    m_cg_processed = true;

    m_progressBar.stop();
}

void GraphAnalyzer::dumpPTACallGraph(const String &filename, bool processing /*=true*/)
{
    if (m_callgraph == nullptr) throw AnalyException("The pointer to call graph is null");
    if (!processing) dumpRawPTACallGraph(filename);
    else dumpProcPTACallGraph(filename);
}

void GraphAnalyzer::dumpPTACallGraph(const String &rawFileName, const String &procFileName)
{
    if (m_callgraph == nullptr) throw AnalyException("The pointer to call graph is null");

    dumpRawPTACallGraph(rawFileName);
    dumpProcPTACallGraph(procFileName);
}

void GraphAnalyzer::updateICFGWithIndirectCalls()
{
    if (m_callgraph == nullptr) throw AnalyException("The pointer to call graph is null");
    if (m_icfg == nullptr) throw AnalyException("The pointer to ICFG is null");

    if (!m_icfg_analyzed) {
        m_icfg->updateCallGraph(m_callgraph);
        m_icfg_analyzed = true;
    }
}

void GraphAnalyzer::dumpRawICFGWithAnalysis(const String &filename)
{
    if (m_icfg == nullptr) throw AnalyException("The pointer to ICFG is null");

    m_progressBar.start(0, "Writing raw ICFG", true);
    m_progressBar.show("Dumping to " + filename + ".dot");

    OutputCapture outCapture;
    outCapture.start();

    m_icfg->dump(filename);

    outCapture.stop();
    m_progressBar.stop();
}

void GraphAnalyzer::dumpProcICFGWithAnalysis(const String &filename)
{
    if (m_icfg == nullptr) throw AnalyException("The pointer to ICFG is null");

    m_progressBar.start(0, "Writing optimized ICFG", true);
    m_progressBar.show("Dumping to " + filename + ".dot");

    String filepath = filename + ".dot";
    std::ofstream outFile(filepath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) throw AnalyException("Failed to open the dot file " + filepath);

    Set<SVF::NodeID> visited;
    outFile << "digraph \"ICFG\" {" << std::endl;
    outFile << "\tlabel=\"ICFG\";\n" << std::endl;
    for (auto iter = m_icfg->begin(), _iter = m_icfg->end(); iter != _iter; ++iter) {
        auto nodeID = iter->second->getId();
        auto nodeIDStr = getNodeIDString(nodeID);
        auto nodeKind = (SVF::ICFGNode::ICFGNodeK)iter->second->getNodeKind();
        if (visited.find(nodeID) == visited.end()) {
            visited.emplace(nodeID);
            unsigned line = 0, column = 0;
            std::string file("");
            switch (nodeKind) {
            case SVF::ICFGNode::ICFGNodeK::GlobalBlock:
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << "];" << std::endl;
                break;
            case SVF::ICFGNode::ICFGNodeK::FunEntryBlock: {
                auto funcEntryNode =
                    SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(iter->second);
                std::string funcName = funcEntryNode->getFun()->getName();
                parseSVFLocationString(
                    funcEntryNode->getBB()->getSourceLoc(), line, column, file
                );
                auto exitNodeID = m_icfg->getFunExitICFGNode(funcEntryNode->getFun())->getId();
                auto exitNodeIDStr = getNodeIDString(exitNodeID);
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << ",function=\""
                        << funcName << "\",line=" << line << ",column=" << column
                        << ",file=" << file << ",corres=" << exitNodeIDStr << "];" << std::endl;
            } break;
            case SVF::ICFGNode::ICFGNodeK::FunExitBlock: {
                auto funcExitNode = SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(iter->second);
                String funcName = funcExitNode->getFun()->getName();
                parseSVFLocationString(
                    funcExitNode->getBB()->getSourceLoc(), line, column, file
                );
                String succLabel = "";
                for (auto t_iter = funcExitNode->OutEdgeBegin();
                     t_iter != funcExitNode->OutEdgeEnd(); ++t_iter)
                {
                    auto succNode = (*t_iter)->getDstNode();
                    if (succNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunRetBlock) {
                        auto succRetNode = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(succNode);
                        auto succCorresCallNode = succRetNode->getCallICFGNode();
                        succLabel += getNodeIDString(succCorresCallNode->getId()) + ":" +
                                     getNodeIDString(succRetNode->getId()) + ";";
                    }
                }
                if (!succLabel.empty() && succLabel.back() == ';') succLabel.pop_back();
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << ",function=\""
                        << funcName << "\",line=" << line << ",column=" << column
                        << ",file=" << file << ",succ=\"" << succLabel << "\"];" << std::endl;
            } break;
            case SVF::ICFGNode::ICFGNodeK::FunCallBlock: {
                auto funcCallNode = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(iter->second);
                String funcName = funcCallNode->getFun()->getName();
                parseSVFLocationString(
                    funcCallNode->getCallSite()->getSourceLoc(), line, column, file
                );

                auto corresNodeIDStr = getNodeIDString(funcCallNode->getRetICFGNode()->getId());
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << ",function=\""
                        << funcName << "\",line=" << line << ",column=" << column
                        << ",file=" << file << ",corres=" << corresNodeIDStr << "];"
                        << std::endl;
            } break;
            case SVF::ICFGNode::ICFGNodeK::FunRetBlock: {
                auto funcRetNode = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(iter->second);
                String funcName = funcRetNode->getFun()->getName();
                parseSVFLocationString(
                    funcRetNode->getCallSite()->getSourceLoc(), line, column, file
                );
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << ",function=\""
                        << funcName << "\",line=" << line << ",column=" << column
                        << ",file=" << file << "];" << std::endl;
            } break;
            case SVF::ICFGNode::ICFGNodeK::IntraBlock: {
                auto intraNode = SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(iter->second);
                String funcName = intraNode->getFun()->getName();
                parseSVFLocationString(
                    intraNode->getInst()->getSourceLoc(), line, column, file
                );
                outFile << "\t" << nodeIDStr << " [type=" << nodeKind << ",function=\""
                        << funcName << "\",line=" << line << ",column=" << column
                        << ",file=" << file << "];" << std::endl;
            } break;
            default:
                throw AnalyException(String("Unknown Node Kind") + std::to_string(nodeKind));
                break;
            }
        }
        for (auto t_iter = iter->second->OutEdgeBegin(), _t_iter = iter->second->OutEdgeEnd();
             t_iter != _t_iter; ++t_iter)
        {
            auto targetNode = (*t_iter)->getDstNode();
            auto targetNodeIDStr = getNodeIDString(targetNode->getId());
            outFile << "\t" << nodeIDStr << " -> " << targetNodeIDStr << " ;" << std::endl;
        }
    }
    outFile << "}" << std::endl;
    outFile.close();
    m_icfg_processed = true;

    m_progressBar.stop();
}

void GraphAnalyzer::dumpICFGWithAnalysis(const String &filename, bool processing /*=true*/)
{
    updateICFGWithIndirectCalls();

    if (processing) dumpICFGWithAnalysis(filename);
    else dumpRawICFGWithAnalysis(filename);
}

void GraphAnalyzer::dumpICFGWithAnalysis(const String &rawFileName, const String &procFileName)
{
    updateICFGWithIndirectCalls();

    dumpRawICFGWithAnalysis(rawFileName);
    dumpProcICFGWithAnalysis(procFileName);
}

GraphAnalyzer::NodeLocation::NodeLocation(
    unsigned _line, unsigned _column, const String &_filePath
)
{
    line = _line;
    column = _column;
    file = _filePath;
    filePathChunks = splitString(_filePath, "/");
}

GraphAnalyzer::NodeLocation::NodeLocation(const String &sourceLoc)
{
    parseSVFLocationString(sourceLoc, line, column, file);
    filePathChunks = splitString(file, "/");
}

GraphAnalyzer::TargetLocation::
    TargetLocation(unsigned _line, const String &_filePath, double _weight /*=1.0*/)
{
    line = _line;
    if (!_filePath.empty()) filePathChunks = splitString(_filePath, "/");
    weight = _weight;
}

GraphAnalyzer::TargetLocation::TargetLocation(const String &targetLoc)
{
    size_t pos = targetLoc.find_last_of(':');
    if (pos == String::npos) throw AnalyException("Invalid target location " + targetLoc);

    auto filePath = targetLoc.substr(0, pos);
    filePath = trimString(filePath);
    if (!filePath.empty()) filePathChunks = splitString(filePath, "/");
    else throw AnalyException("Invalid target location " + targetLoc);

    auto lineStr = targetLoc.substr(pos + 1);
    lineStr = trimString(lineStr);
    if (!lineStr.empty()) line = std::stoul(lineStr);
    else throw AnalyException("Invalid target location " + targetLoc);
}

bool GraphAnalyzer::TargetLocation::isTarget(unsigned _line, const String &_filePath)
{
    if (line != _line) return false;
    auto _filePathChunks = splitString(_filePath, "/");
    if (_filePathChunks.empty()) return false;
    if (filePathChunks.back() != _filePathChunks.back()) return false;
    if (filePathChunks.size() >= 2 && _filePathChunks.size() >= 2 &&
        filePathChunks[filePathChunks.size() - 2] !=
            _filePathChunks[_filePathChunks.size() - 2])
        return false;
    return true;
}

bool GraphAnalyzer::TargetLocation::isTarget(const String &sourceLoc)
{
    unsigned _line = 0, _column = 0;
    String _file = "";

    parseSVFLocationString(sourceLoc, _line, _column, _file);

    return isTarget(_line, _file);
}

bool GraphAnalyzer::TargetLocation::isTarget(const NodeLocation &nodeLoc)
{
    if (line != nodeLoc.line) return false;
    if (nodeLoc.filePathChunks.empty()) return false;
    if (filePathChunks.back() != nodeLoc.filePathChunks.back()) return false;
    if (filePathChunks.size() >= 2 && nodeLoc.filePathChunks.size() >= 2 &&
        filePathChunks[filePathChunks.size() - 2] !=
            nodeLoc.filePathChunks[nodeLoc.filePathChunks.size() - 2])
        return false;
    return true;
}

bool GraphAnalyzer::TargetLocation::isNotTarget(const NodeLocation &nodeLoc)
{
    if (nodeLoc.filePathChunks.empty()) return false;
    if (nodeLoc.line == 0) return false;
    return !isTarget(nodeLoc);
}

void GraphAnalyzer::loadTargets(const String &targetFile)
{
    if (!m_isTargetsLoaded) {
        updateICFGWithIndirectCalls();

        std::ifstream ifs(targetFile, std::ios::in);
        if (!ifs.is_open()) throw AnalyException("Failed to open " + targetFile);

        m_targetLocations.clear();
        m_targetNodes.clear();
        m_targetCount = 0;
        if (targetFile.size() >= 5 && targetFile.substr(targetFile.size() - 5) == ".json") {
            Json::Value root;
            Json::CharReaderBuilder builder;
            builder["collectComments"] = true;
            JSONCPP_STRING errs;
            if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
                throw AnalyException(
                    "Failed to parse json file '" + targetFile + "'. Error: " + errs
                );
            }

            if (!root.isArray())
                throw AnalyException("Invalid format of target file " + targetFile);

            for (Json::Value::ArrayIndex i = 0; i < root.size(); ++i) {
                if (!root[i].isMember("line"))
                    throw AnalyException("Invalid format of target file " + targetFile);
                if (!root[i].isMember("file"))
                    throw AnalyException("Invalid format of target file " + targetFile);
                m_targetLocations.push_back(
                    TargetLocation(root[i]["line"].asUInt(), root[i]["file"].asString())
                );
            }
        }
        else {
            String line;
            while (std::getline(ifs, line)) {
                if (!trimString(line).empty())
                    m_targetLocations.push_back(TargetLocation(line));
            }
        }

        ifs.close();

        if (m_targetLocations.empty()) throw AnalyException("No target was found");
        if (m_targetLocations.size() > MAX_TARGET_COUNT)
            throw AnalyException(
                toString(m_targetLocations.size()) + " targets (more than " +
                toString(MAX_TARGET_COUNT) + ") were found"
            );
        for (size_t i = 0; i < m_targetLocations.size(); ++i) {
            String tmpSrcFilePath = m_projRootPath;
            for (const auto &tmpFileChunk : m_targetLocations[i].filePathChunks) {
                tmpSrcFilePath = joinPath(tmpSrcFilePath, tmpFileChunk);
            }
            if (!pathExists(tmpSrcFilePath) || !pathIsFile(tmpSrcFilePath))
                throw AnalyException(
                    "The source file '" + tmpSrcFilePath + "' of Target " + toString(i) +
                    " doesn't exist"
                );
        }

        m_targetCount = m_targetLocations.size();
        m_targetNodes.resize(m_targetCount);

        for (auto iter = m_icfg->begin(); iter != m_icfg->end(); ++iter) {
            auto currentNode = iter->second;
            auto currentNodeId = iter->first;

            if (currentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::GlobalBlock) continue;

            // Node location
            NodeLocation currentNodeLoc;
            if (m_nodeLocations.find(currentNodeId) == m_nodeLocations.end()) {
                switch (currentNode->getNodeKind()) {
                case SVF::ICFGNode::ICFGNodeK::FunEntryBlock: {
                    auto tmpCurrentNode =
                        SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(currentNode);
                    currentNodeLoc = NodeLocation(tmpCurrentNode->getBB()->getSourceLoc());
                } break;
                case SVF::ICFGNode::ICFGNodeK::FunExitBlock: {
                    auto tmpCurrentNode =
                        SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(currentNode);
                    currentNodeLoc = NodeLocation(tmpCurrentNode->getBB()->getSourceLoc());
                } break;
                case SVF::ICFGNode::ICFGNodeK::FunCallBlock: {
                    auto tmpCurrentNode =
                        SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(currentNode);
                    currentNodeLoc =
                        NodeLocation(tmpCurrentNode->getCallSite()->getSourceLoc());
                } break;
                case SVF::ICFGNode::ICFGNodeK::FunRetBlock: {
                    auto tmpCurrentNode = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(currentNode);
                    currentNodeLoc =
                        NodeLocation(tmpCurrentNode->getCallSite()->getSourceLoc());
                } break;
                case SVF::ICFGNode::ICFGNodeK::IntraBlock: {
                    auto tmpCurrentNode =
                        SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(currentNode);
                    currentNodeLoc = NodeLocation(tmpCurrentNode->getInst()->getSourceLoc());
                } break;
                default:
                    throw AnalyException(
                        "Unknown node kind " + toString(currentNode->getNodeKind())
                    );
                }

                m_nodeLocations[currentNodeId] = currentNodeLoc;
            }
            else {
                currentNodeLoc = m_nodeLocations[currentNodeId];
            }

            // Check each target
            for (size_t i = 0; i < m_targetCount; ++i) {
                if (m_targetLocations[i].isTarget(currentNodeLoc)) {
                    m_targetNodes[i].emplace(currentNodeId);
                }
            }
        }
        for (size_t i = 0; i < m_targetCount; ++i) {
            if (m_targetNodes[i].empty())
                throw AnalyException(
                    "Failed to find real ICFG nodes related to Target " + toString(i)
                );
        }
        m_isTargetsLoaded = true;
    }
}

void GraphAnalyzer::loadSimpleCallGraph()
{
    if (!m_isSimpleCGLoaded) {
        updateICFGWithIndirectCalls();

        for (auto iter = m_icfg->begin(); iter != m_icfg->end(); ++iter) {
            if (iter->second->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock) {
                auto callee = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(iter->second);

                if (callee->getFun()->isIntrinsic() ||
                    SVF::SVFUtil::isExtCall(callee->getFun()))
                    continue;

                if (m_simpleCallGraph.find(callee) == m_simpleCallGraph.end()) {
                    m_simpleCallGraph[callee] = Set<const SVF::FunEntryICFGNode *>();
                }
                for (auto edge = iter->second->InEdgeBegin(); edge != iter->second->InEdgeEnd();
                     ++edge)
                {
                    auto caller = (*edge)->getSrcNode();
                    if (caller->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock) {
                        auto callerNode = m_icfg->getFunEntryICFGNode(caller->getFun());
                        if (m_simpleCallGraph.find(callerNode) == m_simpleCallGraph.end()) {
                            m_simpleCallGraph[callerNode] =
                                Set<const SVF::FunEntryICFGNode *>();
                        }
                        m_simpleCallGraph[callerNode].emplace(callee);
                    }
                }
            }
        }
        m_isSimpleCGLoaded = true;
    }
}

void GraphAnalyzer::subCalculateCalls(const SVF::FunEntryICFGNode *funcEntryNode)
{
    /// A set for the function entry nodes being processed
    static Set<const SVF::FunEntryICFGNode *> curProcEntryNodes;

    // Throw unexpected errors
    if (funcEntryNode == nullptr)
        throw AnalyException("Unexpected error: function entry node is null");
    if (funcEntryNode->getNodeKind() != SVF::ICFGNode::ICFGNodeK::FunEntryBlock)
        throw AnalyException("Unexpected error: function entry node is wrong");

    // Remove current function call from the dynamic function set
    if (m_dynCallSet.find(funcEntryNode) != m_dynCallSet.end())
        m_dynCallSet.erase(funcEntryNode);

    // If the current function is being processed
    if (curProcEntryNodes.find(funcEntryNode) != curProcEntryNodes.end()) return;

    // If the current function is external
    auto currentFunc = funcEntryNode->getFun();
    if (m_simpleCallGraph.find(funcEntryNode) == m_simpleCallGraph.end()) return;

    // If the current function has been processed
    auto currentFuncName = currentFunc->getName();
    if (m_callDistMap.find(currentFuncName) != m_callDistMap.end()) return;

    curProcEntryNodes.emplace(funcEntryNode);

    // DFS first
    for (auto targetFuncEntryNode : m_simpleCallGraph[funcEntryNode]) {
        // Skip self-invocation
        if (targetFuncEntryNode == funcEntryNode) continue;
        // Skip external function calls and unknown function calls
        if (m_simpleCallGraph.find(targetFuncEntryNode) == m_simpleCallGraph.end()) continue;
        // Calculate recursively
        if (m_callDistMap.find(targetFuncEntryNode->getFun()->getName()) == m_callDistMap.end())
            subCalculateCalls(targetFuncEntryNode);
    }

    /// Intra-distance of current function call
    uint32_t intraDist = UINT32_MAX;

    /// Distances from this function to multiple targets
    Vector<int32_t> targetDist(m_targetCount, -1);

    auto funcExitNode = m_icfg->getFunExitICFGNode(funcEntryNode->getFun());

    // BFS inner current function
    Queue<const SVF::ICFGNode *> workNodeQueue;
    Queue<int32_t> workIntraDistQueue;
    workNodeQueue.push(funcEntryNode);
    workIntraDistQueue.push(1);
    Set<const SVF::ICFGNode *> visitedSet;
    while (!workNodeQueue.empty()) {
        auto bfsCurrentNode = workNodeQueue.front();
        auto bfsCurrentNodeId = bfsCurrentNode->getId();
        auto bfsCurrentIntraDist = workIntraDistQueue.front();
        workNodeQueue.pop();
        workIntraDistQueue.pop();

        // Unexpected global node
        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::GlobalBlock)
            throw AnalyException(
                "Unexpected error: multiple global node " + toString(bfsCurrentNodeId)
            );

        // Current node has been visited
        if (visitedSet.find(bfsCurrentNode) != visitedSet.end()) continue;
        else visitedSet.emplace(bfsCurrentNode);

        // Get current node location
        NodeLocation bfsCurrentNodeLoc;
        if (m_nodeLocations.find(bfsCurrentNodeId) == m_nodeLocations.end())
            throw AnalyException(
                "Unexpected error: failed to find node location of Node " +
                toString(bfsCurrentNodeId)
            );

        // Check whether current node is one of the targets
        for (size_t i = 0; i < m_targetCount; ++i) {
            if (m_targetNodes[i].find(bfsCurrentNodeId) != m_targetNodes[i].end()) {
                if (targetDist[i] < 0 || bfsCurrentIntraDist < targetDist[i])
                    targetDist[i] = bfsCurrentIntraDist;
            }
        }

        // Analyze the current node according to its kind
        //
        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunExitBlock) {
            if (funcExitNode->getId() == bfsCurrentNodeId) // Function exit node
            {
                if (intraDist > bfsCurrentIntraDist) intraDist = bfsCurrentIntraDist;
            }
        }
        else if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock)
        { // Function call node
            auto tmpCallNode = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(bfsCurrentNode);
            for (auto iter = bfsCurrentNode->OutEdgeBegin();
                 iter != bfsCurrentNode->OutEdgeEnd(); ++iter)
            {
                auto maybeEntryNode = (*iter)->getDstNode();
                auto tmpCurrentDist = bfsCurrentIntraDist;
                if (maybeEntryNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunRetBlock) {
                    tmpCurrentDist += EXTERN_CALL_DIST;
                }
                else if (maybeEntryNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock)
                {
                    auto tmpEntryNode =
                        SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(maybeEntryNode);
                    auto tmpIter = m_callDistMap.find(tmpEntryNode->getFun()->getName());
                    if (tmpIter != m_callDistMap.end()) {
                        getLesserVector(
                            targetDist, tmpIter->second.second, m_targetCount,
                            bfsCurrentIntraDist
                        );
                        tmpCurrentDist += tmpIter->second.first;
                    }
                    else {
                        // Failed to find the distance for this call
                        // in the map, thus considering it as a
                        // recursive call and using a contant distance
                        tmpCurrentDist += RECURSIVE_CALL_DIST;
                    }
                }
                auto nextNode = tmpCallNode->getRetICFGNode();
                workNodeQueue.push(nextNode);
                workIntraDistQueue.push(tmpCurrentDist);
            }
        }
        else {
            for (auto iter = bfsCurrentNode->OutEdgeBegin();
                 iter != bfsCurrentNode->OutEdgeEnd(); ++iter)
            {
                workNodeQueue.push((*iter)->getDstNode());
                workIntraDistQueue.push(bfsCurrentIntraDist + 1);
            }
        }
    }

    m_callDistMap[currentFuncName] = {intraDist, targetDist};
    curProcEntryNodes.erase(funcEntryNode);

    m_progressBar.show(currentFuncName);
}

void GraphAnalyzer::calculateCallsInICFG(const String &targetFile)
{
    if (m_isCallDistCalc) return;

    if (m_icfg == nullptr) throw AnalyException("The pointer to ICFG is null");

    // Update ICFG with indirect calls
    updateICFGWithIndirectCalls();

    // Load target file
    loadTargets(targetFile);

    // Load simple CG from PTA call graph
    loadSimpleCallGraph();

    m_progressBar.start(m_simpleCallGraph.size(), "Calculating distances for function calls");

    // Load dynamic set for function calls
    m_dynCallSet.clear();
    for (auto &key_value : m_simpleCallGraph) m_dynCallSet.emplace(key_value.first);

    // Process function calls from 'main' function by DFS and BFS
    auto globalNode = m_icfg->getGlobalICFGNode();
    for (auto iter = globalNode->OutEdgeBegin(); iter != globalNode->OutEdgeEnd(); ++iter) {
        auto tmpNode = (*iter)->getDstNode();
        if (tmpNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock) {
            uint32_t intraDist = 0;
            Vector<int32_t> targetDist;
            auto funcEntryNode = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(tmpNode);
            subCalculateCalls(funcEntryNode);
        }
    }

    // Process function calls that don't exist in the complete call chains
    while (!m_dynCallSet.empty()) {
        subCalculateCalls(*(m_dynCallSet.begin()));
    }

    m_progressBar.stop();
    m_isCallDistCalc = true;
}

void GraphAnalyzer::dumpCallsDistance(const String &outCallsDistFile)
{
    String filePath = outCallsDistFile + ".json";

    m_progressBar.start(0, "Writing distances for function calls", true);
    m_progressBar.show("Dumping to " + filePath);

    Json::Value root, jsonTargetNodes, jsonCallsDist;
    jsonTargetNodes.resize(m_targetNodes.size());
    for (Json::Value::ArrayIndex i = 0; i < m_targetNodes.size(); i++) {
        jsonTargetNodes[i].resize(m_targetNodes[i].size());
        Json::Value::ArrayIndex j = 0;
        for (auto nodeID : m_targetNodes[i]) {
            jsonTargetNodes[i][j++] = nodeID;
        }
    }
    for (auto &key_value : m_callDistMap) {
        jsonCallsDist[key_value.first] = Json::Value();
        jsonCallsDist[key_value.first].resize(2);
        jsonCallsDist[key_value.first][0] = key_value.second.first;
        jsonCallsDist[key_value.first][1].resize(key_value.second.second.size());
        for (Json::Value::ArrayIndex i = 0; i < key_value.second.second.size(); ++i) {
            jsonCallsDist[key_value.first][1][i] = key_value.second.second[i];
        }
    }
    root["TargetNodes"] = jsonTargetNodes;
    root["CallDistances"] = jsonCallsDist;

    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) throw AnalyException("Failed to open output file " + filePath);
    Json::StreamWriterBuilder builder;
    const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &ofs);

    m_progressBar.stop();
}

Vector<int32_t> GraphAnalyzer::singleCalculateBlock(const SVF::ICFGNode *node)
{
    Vector<int32_t> result(m_targetCount, -1);
    Queue<const SVF::ICFGNode *> workNodeQueue;
    Queue<int32_t> workIntraDistQueue;
    workNodeQueue.push(node);
    workIntraDistQueue.push(1);
    Set<const SVF::ICFGNode *> visitedSet;
    while (!workNodeQueue.empty()) {
        auto bfsCurrentNode = workNodeQueue.front();
        auto bfsCurrentIntraDist = workIntraDistQueue.front();
        workNodeQueue.pop();
        workIntraDistQueue.pop();

        // Check whether the current node has been visited
        if (visitedSet.find(bfsCurrentNode) != visitedSet.end()) continue;
        else visitedSet.emplace(bfsCurrentNode);

        // Check whether the current node is one of the targets
        for (size_t i = 0; i < m_targetCount; ++i) {
            if (m_targetNodes[i].find(bfsCurrentNode->getId()) != m_targetNodes[i].end()) {
                if (bfsCurrentNode->getNodeKind() != SVF::ICFGNode::ICFGNodeK::FunRetBlock) {
                    if (result[i] < 0 || result[i] > bfsCurrentIntraDist)
                        result[i] = bfsCurrentIntraDist;
                }
            }
        }

        // Analyze the current node according to its kind
        //
        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunExitBlock) {
            // Nothing to do
        }
        else if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock) {
            auto tmpCallNode = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(bfsCurrentNode);
            for (auto iter = bfsCurrentNode->OutEdgeBegin();
                 iter != bfsCurrentNode->OutEdgeEnd(); ++iter)
            {
                auto maybeEntryNode = (*iter)->getDstNode();
                auto tmpCurrentDist = bfsCurrentIntraDist;
                if (maybeEntryNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunRetBlock) {
                    tmpCurrentDist += EXTERN_CALL_DIST;
                }
                else if (maybeEntryNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock)
                {
                    auto tmpIter = m_callDistMap.find(maybeEntryNode->getFun()->getName());
                    if (tmpIter == m_callDistMap.end()) {
                        tmpCurrentDist += RECURSIVE_CALL_DIST;
                    }
                    else {
                        getLesserVector(
                            result, tmpIter->second.second, m_targetCount, tmpCurrentDist
                        );
                        tmpCurrentDist += tmpIter->second.first;
                    }
                }
                auto nextNode = tmpCallNode->getRetICFGNode();
                workNodeQueue.push(nextNode);
                workIntraDistQueue.push(tmpCurrentDist);
            }
        }
        else {
            for (auto iter = bfsCurrentNode->OutEdgeBegin();
                 iter != bfsCurrentNode->OutEdgeEnd(); ++iter)
            {
                workNodeQueue.push((*iter)->getDstNode());
                workIntraDistQueue.push(bfsCurrentIntraDist + 1);
            }
        }
    }

    return result;
}

void GraphAnalyzer::threadCalculateBlocks(const SVF::FunEntryICFGNode *funcEntryNode)
{
    Queue<const SVF::ICFGNode *> workNodeQueue;
    workNodeQueue.push(funcEntryNode);
    Set<const SVF::ICFGNode *> visitedSet;
    while (!workNodeQueue.empty()) {
        auto bfsCurrentNode = workNodeQueue.front();
        workNodeQueue.pop();

        // Check whether the current node has been visited
        if (visitedSet.find(bfsCurrentNode) != visitedSet.end()) continue;
        else visitedSet.emplace(bfsCurrentNode);

        // Get a sequence of nodes with only one parent and one successor
        List<const SVF::ICFGNode *> sequenceNodes;
        if (bfsCurrentNode->getOutEdges().size() == 1) {
            auto tmpNode = bfsCurrentNode;
            do {
                if (tmpNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunExitBlock) break;
                if (tmpNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock) break;
                sequenceNodes.push_back(tmpNode);
                auto tmpIter = tmpNode->OutEdgeBegin();
                tmpNode = (*tmpIter)->getDstNode();
                visitedSet.emplace(tmpNode);
            } while (tmpNode->getOutEdges().size() == 1 && tmpNode->getInEdges().size() == 1);
            bfsCurrentNode = tmpNode;
        }

        // Calculate or get result distances for current node
        Vector<int32_t> bfsCurrentResult(m_targetCount, -1);
        bool hasToCalculate = true;
        {
            UniqueLock lock(m_blockDistMutex);
            if (m_blockDistMap.find(bfsCurrentNode->getId()) != m_blockDistMap.end()) {
                bfsCurrentResult = m_blockDistMap[bfsCurrentNode->getId()];
                hasToCalculate = false;
            }
        }
        if (hasToCalculate) {
            bfsCurrentResult = singleCalculateBlock(bfsCurrentNode);
        }

        // Store result distances
        {
            UniqueLock lock(m_blockDistMutex);
            m_blockDistMap[bfsCurrentNode->getId()] = bfsCurrentResult;
        }
        if (sequenceNodes.size() > 0) {
            auto tmpIter = --sequenceNodes.end();
            while (true) {
                auto tmpNodeID = (*tmpIter)->getId();
                for (size_t i = 0; i < m_targetCount; ++i) {
                    if (bfsCurrentResult[i] >= 0) bfsCurrentResult[i]++;
                    if (m_targetNodes[i].find(tmpNodeID) != m_targetNodes[i].end())
                        bfsCurrentResult[i] = 0;
                }

                {
                    UniqueLock lock(m_blockDistMutex);
                    m_blockDistMap[tmpNodeID] = bfsCurrentResult;
                }

                if (tmpIter == sequenceNodes.begin()) break;
                else --tmpIter;
            }
        }

        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunExitBlock) {
            // Nothing to do
        }
        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock) {
            workNodeQueue.push(
                SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(bfsCurrentNode)->getRetICFGNode()
            );
        }
        else {
            for (auto iter = bfsCurrentNode->OutEdgeBegin();
                 iter != bfsCurrentNode->OutEdgeEnd(); ++iter)
            {
                workNodeQueue.push((*iter)->getDstNode());
            }
        }
    }

    // Map<SVF::NodeID, Vector<int32_t>> distMap;
    // Set<SVF::NodeID> curProcNodes;

    // dfsCalculateBlocks(funcEntryNode, distMap, curProcNodes);

    // {
    //     UniqueLock lock(m_blockDistMutex);
    //     for (auto &key_value : distMap)
    //     {
    //         m_blockDistMap[key_value.first] = key_value.second;
    //     }
    // }

    m_progressBar.show(funcEntryNode->getFun()->getName());
}

void GraphAnalyzer::calculateBlocksPreDistInICFG()
{
    if (m_isBlockDistCalc) return;

    m_progressBar.start(
        m_simpleCallGraph.size(),
        "Calculating pre-completion distances for blocks in functions in ICFG"
    );

    ThreadPool threadPool;
    threadPool.init();
    Vector<std::future<void>> futureVec;
    for (auto iter = m_simpleCallGraph.begin(); iter != m_simpleCallGraph.end(); ++iter) {
        futureVec.push_back(threadPool.submit(
            [this](const SVF::FunEntryICFGNode *funcEntryNode) {
                this->threadCalculateBlocks(funcEntryNode);
            },
            iter->first
        ));
    }
    for (auto &tmpFuture : futureVec) {
        tmpFuture.get();
    }
    threadPool.shutdown();

    // for (auto iter = m_simpleCallGraph.begin(); iter != m_simpleCallGraph.end(); ++iter)
    // {
    //     threadCalculateBlocks(iter->first);
    // }

    m_progressBar.stop();
    m_isBlockDistCalc = true;
}

void GraphAnalyzer::subCalculateFinalBlocks(const SVF::FunEntryICFGNode *funcEntryNode)
{
    // Remove current function from the dynamic set
    if (m_dynCallSet.find(funcEntryNode) != m_dynCallSet.end())
        m_dynCallSet.erase(funcEntryNode);

    bool hasOneSuccessor = false;
    auto funcExitNode = m_icfg->getFunExitICFGNode(funcEntryNode->getFun());
    if (funcExitNode->getOutEdges().size() == 0) return;
    else if (funcExitNode->getOutEdges().size() == 1) hasOneSuccessor = true;

    Vector<int32_t> succTargetDist(m_targetCount, -1);
    Vector<int32_t> pseudoTargetDist(m_targetCount, -1);
    for (auto iter = funcExitNode->OutEdgeBegin(); iter != funcExitNode->OutEdgeEnd(); ++iter) {
        auto curSuccNode = (*iter)->getDstNode();
        auto curSuccNodeID = curSuccNode->getId();
        if (m_blockDistMap.find(curSuccNodeID) != m_blockDistMap.end()) {
            getLesserVector(succTargetDist, m_blockDistMap[curSuccNodeID], m_targetCount);
            getLesserVector(pseudoTargetDist, m_blockDistMap[curSuccNodeID], m_targetCount);
        }
        if (m_blockPseudoDistMap.find(curSuccNodeID) != m_blockPseudoDistMap.end()) {
            getLesserVector(
                pseudoTargetDist, m_blockPseudoDistMap[curSuccNodeID], m_targetCount
            );
        }
    }

    Queue<const SVF::ICFGNode *> workNodeQueue;
    workNodeQueue.push(funcExitNode);
    Queue<int32_t> workIntraDistQueue;
    workIntraDistQueue.push(1);
    Set<const SVF::ICFGNode *> visitedNodes;
    while (!workNodeQueue.empty()) {
        auto bfsCurrentNode = workNodeQueue.front();
        auto bfsCurrentIntraDist = workIntraDistQueue.front();
        workNodeQueue.pop();
        workIntraDistQueue.pop();
        auto bfsCurrentNodeId = bfsCurrentNode->getId();

        if (visitedNodes.find(bfsCurrentNode) != visitedNodes.end()) continue;
        else visitedNodes.emplace(bfsCurrentNode);

        if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock) {
            // Nothing to do
        }
        else {
            // Add distances to block distances
            auto tmpTargetDist = succTargetDist;
            updateVectorWithDelta(tmpTargetDist, bfsCurrentIntraDist);
            // for (auto &value : tmpTargetDist)
            //     value = value < 0 ? value : (value + bfsCurrentIntraDist);
            if (hasOneSuccessor) {
                if (m_blockDistMap.find(bfsCurrentNodeId) == m_blockDistMap.end())
                    m_blockDistMap[bfsCurrentNodeId] = tmpTargetDist;
                else
                    getNonNegativeVector(
                        m_blockDistMap[bfsCurrentNodeId], tmpTargetDist, m_targetCount
                    );
            }

            // Add distances to block pseudo-distances
            tmpTargetDist = pseudoTargetDist;
            updateVectorWithDelta(tmpTargetDist, bfsCurrentIntraDist);
            // for (auto &value : tmpTargetDist)
            //     value = value < 0 ? value : (value + bfsCurrentIntraDist);
            if (m_blockPseudoDistMap.find(bfsCurrentNodeId) == m_blockPseudoDistMap.end())
                m_blockPseudoDistMap[bfsCurrentNodeId] = tmpTargetDist;
            else
                getLesserVector(
                    m_blockPseudoDistMap[bfsCurrentNodeId], tmpTargetDist, m_targetCount
                );

            // Add ICFG nodes to working queue
            if (bfsCurrentNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunRetBlock) {
                auto tmpRetNode = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(bfsCurrentNode);
                for (auto iter = bfsCurrentNode->InEdgeBegin();
                     iter != bfsCurrentNode->InEdgeEnd(); ++iter)
                {
                    auto tmpIntraDist = bfsCurrentIntraDist;
                    auto maybeExitNode = (*iter)->getSrcNode();
                    if (maybeExitNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunCallBlock)
                    {
                        tmpIntraDist += EXTERN_CALL_DIST;
                    }
                    else if (maybeExitNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunExitBlock)
                    {
                        auto tmpFuncName = (*iter)->getSrcNode()->getFun()->getName();
                        if (m_callDistMap.find(tmpFuncName) != m_callDistMap.end()) {
                            tmpIntraDist += m_callDistMap[tmpFuncName].first;
                        }
                    }
                    workNodeQueue.push(tmpRetNode->getCallICFGNode());
                    workIntraDistQueue.push(tmpIntraDist);
                }
            }
            else {
                for (auto iter = bfsCurrentNode->InEdgeBegin();
                     iter != bfsCurrentNode->InEdgeEnd(); ++iter)
                {
                    workNodeQueue.push((*iter)->getSrcNode());
                    workIntraDistQueue.push(bfsCurrentIntraDist + 1);
                }
            }
        }
    }
}

void GraphAnalyzer::calculateBlocksFinalDistInICFG()
{
    if (m_isPseudoDistCalc) return;

    Queue<const SVF::FunEntryICFGNode *> workEntryNodeQueue;
    Set<const SVF::FunEntryICFGNode *> visitedEntryNodes;
    auto globalNode = m_icfg->getGlobalICFGNode();
    for (auto iter = globalNode->OutEdgeBegin(); iter != globalNode->OutEdgeEnd(); ++iter) {
        auto dstNode = (*iter)->getDstNode();
        if (dstNode->getNodeKind() == SVF::ICFGNode::ICFGNodeK::FunEntryBlock) {
            workEntryNodeQueue.push(SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(dstNode));
        }
    }

    m_progressBar.start(
        m_simpleCallGraph.size(), "Calculating final distances for blocks in functions in ICFG"
    );

    // Load the dynamic set for function calls
    m_dynCallSet.clear();
    for (auto &key_value : m_simpleCallGraph) m_dynCallSet.emplace(key_value.first);

    // BFS
    while (!m_dynCallSet.empty()) {
        // Process function calls that don't exist in call chains
        if (workEntryNodeQueue.empty()) {
            auto iter = m_dynCallSet.begin();
            for (; iter != m_dynCallSet.end(); ++iter) {
                if ((*iter)->getFun()->isUncalledFunction()) break;
            }
            iter = iter == m_dynCallSet.end() ? m_dynCallSet.begin() : iter;
            workEntryNodeQueue.push(*iter);
        }

        while (!workEntryNodeQueue.empty()) {
            auto bfsCurrentEntryNode = workEntryNodeQueue.front();
            workEntryNodeQueue.pop();

            if (visitedEntryNodes.find(bfsCurrentEntryNode) != visitedEntryNodes.end())
                continue;
            else visitedEntryNodes.emplace(bfsCurrentEntryNode);

            // Calculate
            subCalculateFinalBlocks(bfsCurrentEntryNode);

            m_progressBar.show(bfsCurrentEntryNode->getFun()->getName());

            for (const auto &nextEntryNode : m_simpleCallGraph[bfsCurrentEntryNode]) {
                workEntryNodeQueue.push(nextEntryNode);
            }
        }
    }

    m_progressBar.stop();

    m_isPseudoDistCalc = true;
}

String GraphAnalyzer::getRelSrcFilePath(
    const String &fileName, const StringVector &fileNameChunks
)
{
    static Map<String, String> fileNameMap;

    if (fileName.empty()) return "";

    if (fileNameMap.find(fileName) != fileNameMap.end()) return fileNameMap[fileName];

    size_t pos = 0;
    while (pos < fileNameChunks.size()) {
        if (fileNameChunks[pos] != ".." && fileNameChunks[pos] != ".") break;
        else ++pos;
    }
    if (pos >= fileNameChunks.size()) {
        fileNameMap[fileName] = "";
        return "";
    }

    String relSrcFilePath = "";
    while (pos < fileNameChunks.size()) {
        relSrcFilePath += fileNameChunks[pos++];
        if (pos < fileNameChunks.size()) relSrcFilePath += '/';
    }

    String simFilePath = m_projRootPath + "/" + relSrcFilePath;
    if (pathExists(simFilePath) && pathIsFile(simFilePath)) {
        fileNameMap[fileName] = relSrcFilePath;
        return relSrcFilePath;
    }
    else {
        relSrcFilePath = fileNameChunks.back();
        fileNameMap[fileName] = relSrcFilePath;
        return relSrcFilePath;
    }
}

String GraphAnalyzer::getRelSrcFilePath(const String &fileName)
{
    return getRelSrcFilePath(fileName, splitString(fileName, "/"));
}

void GraphAnalyzer::dumpBlocksDistance(
    const String &outBlocksDistFile, bool isPseudo /*=false*/
)
{
    String filePath = outBlocksDistFile + ".json";

    if (!isPseudo)
        m_progressBar.start(0, "Writing depth-first distances for blocks in ICFG", true);
    else m_progressBar.start(0, "Writing backtrace distances for blocks in ICFG", true);
    m_progressBar.show("Dumping to " + filePath);

    Map<SVF::NodeID, Vector<int32_t>> tmpBlockDistMap;
    if (!isPseudo) tmpBlockDistMap = m_blockDistMap;
    else tmpBlockDistMap = m_blockPseudoDistMap;

    Json::Value root;
    for (auto &key_value : tmpBlockDistMap) {
        if (m_nodeLocations.find(key_value.first) != m_nodeLocations.end()) {
            auto file = getRelSrcFilePath(
                m_nodeLocations[key_value.first].file,
                m_nodeLocations[key_value.first].filePathChunks
            );
            auto line = toString(m_nodeLocations[key_value.first].line);
            if (!file.empty()) {
                if (!root.isMember(file)) root[file] = Json::Value();
                if (!root[file].isMember(line)) {
                    root[file][line] = Json::Value();
                    root[file][line].resize(m_targetCount);
                    for (unsigned i = 0; i < m_targetCount; ++i) {
                        root[file][line][i] = key_value.second[i];
                    }
                }
                else {
                    getLesserVector(root[file][line], key_value.second, m_targetCount);
                }
            }
        }
    }

    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) throw AnalyException("Failed to open output file " + filePath);
    Json::StreamWriterBuilder builder;
    const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &ofs);

    m_progressBar.stop();
}

void GraphAnalyzer::dumpBasicBlockDistance(
    const String &outBBDistFile, bool isPseudo /*=false*/
)
{
    String filePath = outBBDistFile + ".json";

    Map<SVF::NodeID, Vector<int32_t>> tmpBlockDistMap;
    if (!isPseudo) {
        m_progressBar.start(0, "Writing depth-first distances for basic blocks", true);
        tmpBlockDistMap = m_blockDistMap;
    }
    else {
        m_progressBar.start(0, "Writing backtrace distances for basic blocks", true);
        tmpBlockDistMap = m_blockPseudoDistMap;
    }
    m_progressBar.show("Dumping to " + filePath);

    Map<const SVF::SVFBasicBlock *, Vector<int32_t>> BBDistMap;
    for (auto &key_value : tmpBlockDistMap) {
        auto node = m_icfg->getICFGNode(key_value.first);
        auto nodeID = node->getId();
        auto nodeBB = node->getBB();
        if (BBDistMap.find(nodeBB) == BBDistMap.end()) {
            BBDistMap[nodeBB] = key_value.second;
        }
        else {
            getLesserVector(BBDistMap[nodeBB], key_value.second, m_targetCount);
        }
    }

    Json::Value root;
    for (auto &key_value : BBDistMap) {
        unsigned line = 0, column = 0;
        String file("");
        String sourceLoc = key_value.first->getSourceLoc();
        parseSVFLocationString(sourceLoc, line, column, file);
        if (!file.empty() && line > 0) {
            String relSrcFilePath = getRelSrcFilePath(file);
            String lineStr = toString(line);
            if (!root.isMember(relSrcFilePath)) root[relSrcFilePath] = Json::Value();
            if (!root[relSrcFilePath].isMember(lineStr)) {
                root[relSrcFilePath][lineStr] = Json::Value();
                root[relSrcFilePath][lineStr].resize(m_targetCount);
                for (Json::Value::ArrayIndex i = 0; i < m_targetCount; ++i) {
                    root[relSrcFilePath][lineStr][i] = key_value.second[i];
                }
            }
            else {
                getLesserVector(root[relSrcFilePath][lineStr], key_value.second, m_targetCount);
            }
        }
    }

    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) throw AnalyException("Failed to open output file " + filePath);
    Json::StreamWriterBuilder builder;
    const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &ofs);

    m_progressBar.stop();
}

void GraphAnalyzer::dumpTargetFuzzingInfo(const String &outFuzzingInfoFile, bool usingDistrib)
{
    /// Calculate frequency of sample data
    auto calcFrequency = [](const Vector<uint32_t> &data, Vector<long double> &quantile,
                            uint32_t &start) {
        auto sortedData = data;
        std::sort(sortedData.begin(), sortedData.end());

        size_t sum = 0;
        size_t index = 0;
        size_t qIndex = 0;
        quantile.resize(sortedData.back() - sortedData.front() + 1);
        while (index < sortedData.size()) {
            auto curNum = sortedData[index];
            auto curPercent = (long double)sum / (long double)sortedData.size();
            quantile[qIndex++] = curPercent;
            while (index < sortedData.size() && curNum == sortedData[index]) {
                index++;
                sum++;
            }
            if (index < sortedData.size()) {
                for (uint32_t k = curNum + 1; k < sortedData[index]; ++k)
                    quantile[qIndex++] = curPercent;
            }
        }
        start = sortedData.front();
    };

    /// Estimate gamma distribution
    auto calcDistribution = [](const Vector<uint32_t> &data, Vector<long double> &quantile,
                               uint32_t &start) {
        auto sortedData = data;
        std::sort(sortedData.begin(), sortedData.end());

        GammaDistrib gamma;
        gamma.estimate(sortedData);

        start = sortedData.front();
        uint32_t cdfStart = start;
        uint32_t cdfEnd = sortedData.back();
        gamma.getCDFQuantile(cdfStart, cdfEnd, quantile);
    };

    String filePath = outFuzzingInfoFile + ".json";

    m_progressBar.start(0, "Writing the target information for fuzzing", true);
    m_progressBar.show("Dumping to " + filePath);

    Json::Value root;
    root["TargetCount"] = m_targetCount;
    root["TargetInfo"] = Json::Value();
    root["TargetInfo"].resize(m_targetCount);

    Vector<Vector<uint32_t>> sampleData(m_targetCount, Vector<uint32_t>());

    Map<const SVF::SVFBasicBlock *, Vector<int32_t>> BBDistMap;
    for (const auto &key_value : m_blockDistMap) {
        auto node = m_icfg->getICFGNode(key_value.first);
        auto nodeID = node->getId();
        auto nodeBB = node->getBB();
        if (BBDistMap.find(nodeBB) == BBDistMap.end()) {
            BBDistMap[nodeBB] = key_value.second;
        }
        else {
            getLesserVector(BBDistMap[nodeBB], key_value.second, m_targetCount);
        }
    }
    for (const auto &key_value : BBDistMap) {
        for (size_t i = 0; i < m_targetCount; ++i) {
            if (key_value.second[i] >= 0) sampleData[i].push_back(key_value.second[i]);
        }
    }

    for (size_t i = 0; i < m_targetCount; ++i) {
        Vector<long double> probQuantile;
        uint32_t probStart;
        Json::Value tmpJsonRoot;
        if (usingDistrib) {
            calcDistribution(sampleData[i], probQuantile, probStart);
            tmpJsonRoot["Method"] = "Estimation";
        }
        else {
            calcFrequency(sampleData[i], probQuantile, probStart);
            tmpJsonRoot["Method"] = "Frequency";
        }
        tmpJsonRoot["Start"] = probStart;
        tmpJsonRoot["Quantile"] = Json::Value();
        tmpJsonRoot["Quantile"].resize(probQuantile.size());
        for (Json::Value::ArrayIndex j = 0; j < probQuantile.size(); ++j) {
            tmpJsonRoot["Quantile"][j] = (double)probQuantile[j];
        }
        root["TargetInfo"][(Json::Value::ArrayIndex)i] = tmpJsonRoot;
    }

    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) throw AnalyException("Failed to open output file " + filePath);
    Json::StreamWriterBuilder builder;
    const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &ofs);

    m_progressBar.stop();
}

} // namespace Analy
} // namespace FGo