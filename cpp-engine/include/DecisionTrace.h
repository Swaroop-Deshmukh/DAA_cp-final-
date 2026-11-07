#ifndef DECISION_TRACE_H
#define DECISION_TRACE_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

/**
 * @brief Node representation for frontend Graph Visualizer.
 */
struct GraphNodeDto {
    int id = 0;
    std::string name;
    std::string type; // "SOURCE", "REGION", "SERVER", "ORIGIN", "SINK"
    double capacity = 0.0;
    double demand = 0.0;
    bool isActive = true;
};

/**
 * @brief Edge representation for frontend Graph Visualizer.
 */
struct GraphEdgeDto {
    int id = 0;
    int fromId = 0;
    int toId = 0;
    std::string fromName;
    std::string toName;
    double capacity = 0.0;
    double flow = 0.0;
    double residualCapacity = 0.0;
    double unitCost = 0.0;
    long long scaledCost = 0;
    bool isReverse = false;
    int reverseEdgeIndex = -1;
    std::string label;
};

/**
 * @brief Records discrete changes to edge flow and residual capacity during an augmentation.
 */
struct EdgeUpdateDto {
    int fromId = 0;
    int toId = 0;
    std::string fromName;
    std::string toName;
    double prevFlow = 0.0;
    double newFlow = 0.0;
    double prevResidual = 0.0;
    double newResidual = 0.0;
    bool isReverse = false;
    long long reducedCost = 0;
    std::string label;
};

/**
 * @brief Records a candidate path evaluated during routing search.
 */
struct CandidatePathDto {
    std::string pathString;
    std::vector<std::string> nodeNames;
    double pathCost = 0.0;
    double bottleneck = 0.0;
    std::string status; // "SELECTED", "EVALUATED", "EXCLUDED_OUTAGE", "EXCLUDED_CAPACITY"
    std::string reason;
};

/**
 * @brief Represents a single discrete decision or computation step during routing algorithm execution.
 */
struct TraceStep {
    int stepNumber = 0;
    std::string phase;          // "GRAPH_INIT", "PATH_SEARCH", "FLOW_AUGMENT", "RESIDUAL_UPDATE", "TERMINATION", "CANDIDATE_EVALUATION", "ASSIGNMENT", "CAPACITY_EXHAUSTED", "FALLBACK"
    std::string action;         // Brief headline
    std::string description;    // Detailed narrative explaining mathematical calculations and rationale
    std::map<std::string, std::string> details; // Key-value metrics for inspection

    // Interactive Graph Visualizer Fields
    std::vector<int> pathNodeIds;
    std::vector<std::string> pathNodeNames;
    double bottleneckFlow = 0.0;
    std::vector<EdgeUpdateDto> edgeUpdates;
    std::vector<CandidatePathDto> candidatePaths;
};

/**
 * @brief Complete execution trace captured during a single routing optimization run.
 */
struct DecisionTrace {
    std::string strategy;
    int totalSteps = 0;
    std::vector<TraceStep> steps;
    std::string finalExplanation;

    // Full baseline network graph topology constructed by C++
    std::vector<GraphNodeDto> initialNodes;
    std::vector<GraphEdgeDto> initialEdges;

    void addStep(int stepNum, const std::string& phase, const std::string& action, 
                 const std::string& desc, const std::map<std::string, std::string>& details = {},
                 const std::vector<int>& nodeIds = {}, const std::vector<std::string>& nodeNames = {},
                 double flowPushed = 0.0, const std::vector<EdgeUpdateDto>& updates = {},
                 const std::vector<CandidatePathDto>& candidates = {}) {
        TraceStep step;
        step.stepNumber = stepNum;
        step.phase = phase;
        step.action = action;
        step.description = desc;
        step.details = details;
        step.pathNodeIds = nodeIds;
        step.pathNodeNames = nodeNames;
        step.bottleneckFlow = flowPushed;
        step.edgeUpdates = updates;
        step.candidatePaths = candidates;
        steps.push_back(step);
        totalSteps = static_cast<int>(steps.size());
    }

    std::string toJson() const {
        auto escapeJson = [](const std::string& s) {
            std::string out = "";
            for (char c : s) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else out += c;
            }
            return out;
        };

        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"strategy\": \"" << escapeJson(strategy) << "\",\n";
        ss << "  \"totalSteps\": " << totalSteps << ",\n";
        ss << "  \"finalExplanation\": \"" << escapeJson(finalExplanation) << "\",\n";

        // 1. Initial Nodes
        ss << "  \"initialNodes\": [\n";
        for (size_t i = 0; i < initialNodes.size(); ++i) {
            const auto& n = initialNodes[i];
            ss << "    {\"id\": " << n.id << ", \"name\": \"" << escapeJson(n.name)
               << "\", \"type\": \"" << escapeJson(n.type)
               << "\", \"capacity\": " << n.capacity
               << ", \"demand\": " << n.demand
               << ", \"isActive\": " << (n.isActive ? "true" : "false") << "}"
               << (i + 1 < initialNodes.size() ? "," : "") << "\n";
        }
        ss << "  ],\n";

        // 2. Initial Edges
        ss << "  \"initialEdges\": [\n";
        for (size_t i = 0; i < initialEdges.size(); ++i) {
            const auto& e = initialEdges[i];
            ss << "    {\"id\": " << e.id << ", \"fromId\": " << e.fromId << ", \"toId\": " << e.toId
               << ", \"fromName\": \"" << escapeJson(e.fromName) << "\", \"toName\": \"" << escapeJson(e.toName)
               << "\", \"capacity\": " << e.capacity << ", \"flow\": " << e.flow
               << ", \"residualCapacity\": " << e.residualCapacity << ", \"unitCost\": " << e.unitCost
               << ", \"scaledCost\": " << e.scaledCost << ", \"isReverse\": " << (e.isReverse ? "true" : "false")
               << ", \"reverseEdgeIndex\": " << e.reverseEdgeIndex << ", \"label\": \"" << escapeJson(e.label) << "\"}"
               << (i + 1 < initialEdges.size() ? "," : "") << "\n";
        }
        ss << "  ],\n";

        // 3. Execution Steps
        ss << "  \"steps\": [\n";
        for (size_t i = 0; i < steps.size(); ++i) {
            const auto& s = steps[i];
            ss << "    {\n";
            ss << "      \"stepNumber\": " << s.stepNumber << ",\n";
            ss << "      \"phase\": \"" << escapeJson(s.phase) << "\",\n";
            ss << "      \"action\": \"" << escapeJson(s.action) << "\",\n";
            ss << "      \"description\": \"" << escapeJson(s.description) << "\",\n";
            ss << "      \"bottleneckFlow\": " << s.bottleneckFlow << ",\n";

            // pathNodeIds
            ss << "      \"pathNodeIds\": [";
            for (size_t j = 0; j < s.pathNodeIds.size(); ++j) {
                ss << s.pathNodeIds[j] << (j + 1 < s.pathNodeIds.size() ? ", " : "");
            }
            ss << "],\n";

            // pathNodeNames
            ss << "      \"pathNodeNames\": [";
            for (size_t j = 0; j < s.pathNodeNames.size(); ++j) {
                ss << "\"" << escapeJson(s.pathNodeNames[j]) << "\"" << (j + 1 < s.pathNodeNames.size() ? ", " : "");
            }
            ss << "],\n";

            // edgeUpdates
            ss << "      \"edgeUpdates\": [";
            for (size_t j = 0; j < s.edgeUpdates.size(); ++j) {
                const auto& u = s.edgeUpdates[j];
                ss << "{\"fromId\": " << u.fromId << ", \"toId\": " << u.toId
                   << ", \"fromName\": \"" << escapeJson(u.fromName) << "\", \"toName\": \"" << escapeJson(u.toName)
                   << "\", \"prevFlow\": " << u.prevFlow << ", \"newFlow\": " << u.newFlow
                   << ", \"prevResidual\": " << u.prevResidual << ", \"newResidual\": " << u.newResidual
                   << ", \"isReverse\": " << (u.isReverse ? "true" : "false")
                   << ", \"reducedCost\": " << u.reducedCost
                   << ", \"label\": \"" << escapeJson(u.label) << "\"}"
                   << (j + 1 < s.edgeUpdates.size() ? ", " : "");
            }
            ss << "],\n";

            // candidatePaths
            ss << "      \"candidatePaths\": [";
            for (size_t j = 0; j < s.candidatePaths.size(); ++j) {
                const auto& cp = s.candidatePaths[j];
                ss << "{\"pathString\": \"" << escapeJson(cp.pathString) << "\", \"pathCost\": " << cp.pathCost
                   << ", \"bottleneck\": " << cp.bottleneck << ", \"status\": \"" << escapeJson(cp.status)
                   << "\", \"reason\": \"" << escapeJson(cp.reason) << "\", \"nodeNames\": [";
                for (size_t k = 0; k < cp.nodeNames.size(); ++k) {
                    ss << "\"" << escapeJson(cp.nodeNames[k]) << "\"" << (k + 1 < cp.nodeNames.size() ? ", " : "");
                }
                ss << "]}" << (j + 1 < s.candidatePaths.size() ? ", " : "");
            }
            ss << "],\n";

            // details
            ss << "      \"details\": {";
            bool firstDet = true;
            for (const auto& pair : s.details) {
                if (!firstDet) ss << ", ";
                firstDet = false;
                ss << "\"" << escapeJson(pair.first) << "\": \"" << escapeJson(pair.second) << "\"";
            }
            ss << "}\n";

            ss << "    }" << (i + 1 < steps.size() ? "," : "") << "\n";
        }
        ss << "  ]\n";
        ss << "}";
        return ss.str();
    }
};

#endif // DECISION_TRACE_H
