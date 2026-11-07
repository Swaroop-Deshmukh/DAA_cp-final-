#include "../include/Algorithms.h"
#include <iostream>
#include <iomanip>
#include <queue>
#include <cmath>
#include <cassert>

// ==========================================
// Potentials Initialization
// ==========================================
bool MinCostMaxFlow::initializePotentials(const Graph& graph, int source, std::vector<long long>& potentials) {
    int n = graph.getNodeCount();
    potentials.assign(n, 0);

    // Check if any initial forward edge has a negative cost
    bool hasNegativeCost = false;
    for (int u = 0; u < n; ++u) {
        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (!edge.isReverse && edge.capacity > EPSILON && edge.unitCost < 0) {
                hasNegativeCost = true;
                break;
            }
        }
        if (hasNegativeCost) break;
    }

    // In CDN flow graphs, initial forward edge costs are strictly non-negative.
    // If no negative edges exist, pi(v) = 0 is already a valid potential.
    if (!hasNegativeCost) {
        return true;
    }

    // For general graphs with negative edge weights, compute initial potentials using Bellman-Ford
    std::vector<long long> dist(n, INF_COST);
    dist[source] = 0;

    for (int iter = 0; iter < n - 1; ++iter) {
        bool anyRelaxed = false;
        for (int u = 0; u < n; ++u) {
            if (dist[u] == INF_COST) continue;
            for (const auto& edge : graph.getEdgesFrom(u)) {
                if (!edge.isReverse && edge.capacity > EPSILON) {
                    int v = edge.to;
                    if (dist[u] + edge.unitCost < dist[v]) {
                        dist[v] = dist[u] + edge.unitCost;
                        anyRelaxed = true;
                    }
                }
            }
        }
        if (!anyRelaxed) break;
    }

    // Negative cycle detection on n-th pass
    for (int u = 0; u < n; ++u) {
        if (dist[u] == INF_COST) continue;
        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (!edge.isReverse && edge.capacity > EPSILON) {
                int v = edge.to;
                if (dist[u] + edge.unitCost < dist[v]) {
                    std::cerr << "[MinCostMaxFlow Error] Negative cost cycle detected in initial graph!\n";
                    return false;
                }
            }
        }
    }

    // Use Bellman-Ford distances as valid initial potentials
    for (int i = 0; i < n; ++i) {
        if (dist[i] < INF_COST) {
            potentials[i] = dist[i];
        }
    }
    return true;
}

// ==========================================
// Dijkstra with Johnson Potentials
// ==========================================
bool MinCostMaxFlow::findCheapestPath(const Graph& graph, int source, int sink,
                                      const std::vector<long long>& potentials,
                                      std::vector<long long>& dist,
                                      std::vector<int>& parentNode,
                                      std::vector<int>& parentEdgeIdx) {
    int n = graph.getNodeCount();
    dist.assign(n, INF_COST);
    parentNode.assign(n, -1);
    parentEdgeIdx.assign(n, -1);

    // Min-priority queue: stores {distance, nodeID}
    std::priority_queue<std::pair<long long, int>, 
                        std::vector<std::pair<long long, int>>, 
                        std::greater<std::pair<long long, int>>> pq;

    dist[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;

        const auto& edges = graph.getEdgesFrom(u);
        for (int eIdx = 0; eIdx < static_cast<int>(edges.size()); ++eIdx) {
            const auto& edge = edges[eIdx];

            // Only traverse edges with available residual capacity
            if (edge.getResidualCapacity() <= EPSILON) continue;

            int v = edge.to;

            // Johnson Reduced Cost Formula:
            // reducedCost(u, v) = cost(u, v) + pi(u) - pi(v)
            long long reducedCost = edge.unitCost + potentials[u] - potentials[v];

            // Invariant: Reduced cost must be non-negative under valid potentials
            if (reducedCost < 0) {
                // Safeguard against minor integer truncation
                reducedCost = 0;
            }

            if (dist[u] + reducedCost < dist[v]) {
                dist[v] = dist[u] + reducedCost;
                parentNode[v] = u;
                parentEdgeIdx[v] = eIdx;
                pq.push({dist[v], v});
            }
        }
    }

    return dist[sink] < INF_COST;
}

// ==========================================
// Update Potentials
// ==========================================
void MinCostMaxFlow::updatePotentials(std::vector<long long>& potentials, const std::vector<long long>& dist) {
    int n = static_cast<int>(potentials.size());
    for (int u = 0; u < n; ++u) {
        if (dist[u] < INF_COST) {
            potentials[u] += dist[u];
        }
    }
}

// ==========================================
// Augment Flow along Path
// ==========================================
double MinCostMaxFlow::sendTraffic(Graph& graph, int source, int sink,
                                  const std::vector<int>& parentNode,
                                  const std::vector<int>& parentEdgeIdx,
                                  double maxAllowedFlow) {
    // 1. Identify the bottleneck residual capacity along the augmenting path
    double delta = std::numeric_limits<double>::max();
    if (maxAllowedFlow > 0.0) {
        delta = maxAllowedFlow;
    }

    int curr = sink;

    while (curr != source) {
        int u = parentNode[curr];
        int eIdx = parentEdgeIdx[curr];
        const FlowEdge& fwdEdge = graph.getEdgesFrom(u)[eIdx];
        delta = std::min(delta, fwdEdge.getResidualCapacity());
        curr = u;
    }

    if (delta <= EPSILON) return 0.0;

    // 2. Augment flow and update residual capacities
    curr = sink;
    while (curr != source) {
        int u = parentNode[curr];
        int eIdx = parentEdgeIdx[curr];

        FlowEdge& fwdEdge = graph.getEdgesFrom(u)[eIdx];
        FlowEdge& revEdge = graph.getEdgesFrom(curr)[fwdEdge.reverseEdgeIndex];

        fwdEdge.flow += delta;
        revEdge.flow -= delta;

        curr = u;
    }

    return delta;
}

// ==========================================
// Full Min-Cost Max-Flow Driver
// ==========================================
MCMFResult MinCostMaxFlow::calculateMinCostMaxFlow(Graph& graph, int source, int sink,
                                                  double targetFlow,
                                                  long long costScalingFactor,
                                                  DecisionTrace* trace) {
    MCMFResult result;
    result.maxFlow = 0.0;
    result.totalContinuousCost = 0.0;
    result.totalScaledCost = 0;
    result.iterations = 0;
    result.isOptimal = false;

    int n = graph.getNodeCount();
    std::vector<long long> potentials(n, 0);

    int stepCounter = 1;
    if (trace) {
        trace->strategy = "Min-Cost Max-Flow";
        trace->finalExplanation = "Min-Cost Max-Flow executes Successive Shortest Path (SSP) with Johnson dual potentials and Dijkstra to achieve the global mathematical minimum routing cost.";

        // Populate initial graph nodes for frontend Graph Visualizer
        trace->initialNodes.clear();
        for (const auto& node : graph.getAllNodes()) {
            GraphNodeDto nDto;
            nDto.id = node.id;
            nDto.name = node.name;
            switch (node.type) {
                case NodeType::SOURCE: nDto.type = "SOURCE"; break;
                case NodeType::REGION: nDto.type = "REGION"; break;
                case NodeType::SERVER: nDto.type = "SERVER"; break;
                case NodeType::ORIGIN: nDto.type = "ORIGIN"; break;
                case NodeType::SINK: nDto.type = "SINK"; break;
            }
            nDto.capacity = 0.0;
            nDto.demand = 0.0;
            nDto.isActive = true;
            trace->initialNodes.push_back(nDto);
        }

        // Populate initial graph edges (both forward and reverse residual)
        trace->initialEdges.clear();
        int edgeCounter = 0;
        for (int u = 0; u < n; ++u) {
            for (const auto& edge : graph.getEdgesFrom(u)) {
                GraphEdgeDto eDto;
                eDto.id = edgeCounter++;
                eDto.fromId = edge.from;
                eDto.toId = edge.to;
                eDto.fromName = graph.getNodeInfo(edge.from).name;
                eDto.toName = graph.getNodeInfo(edge.to).name;
                eDto.capacity = edge.capacity;
                eDto.flow = edge.flow;
                eDto.residualCapacity = edge.getResidualCapacity();
                eDto.unitCost = static_cast<double>(edge.unitCost) / static_cast<double>(costScalingFactor);
                eDto.scaledCost = edge.unitCost;
                eDto.isReverse = edge.isReverse;
                eDto.reverseEdgeIndex = edge.reverseEdgeIndex;
                eDto.label = edge.label;
                trace->initialEdges.push_back(eDto);
            }
        }

        std::map<std::string, std::string> d;
        d["total_nodes"] = std::to_string(n);
        d["super_source"] = graph.getNodeInfo(source).name;
        d["super_sink"] = graph.getNodeInfo(sink).name;
        d["target_flow"] = (targetFlow > 0.0) ? (std::to_string(static_cast<long long>(targetFlow)) + " req/s") : "Max Feasible";
        d["cost_scaling_factor"] = std::to_string(costScalingFactor);
        trace->addStep(stepCounter++, "GRAPH_INIT", "Residual Flow Graph Initialized",
                       "Constructed layered residual flow graph with " + std::to_string(n) + " nodes and " +
                       std::to_string(trace->initialEdges.size()) + " forward/reverse edges. Initialized Johnson dual potentials pi(v) = 0 for all nodes.", d);
    }

    // 1. Initialize node potentials
    if (!initializePotentials(graph, source, potentials)) {
        result.terminationReason = "Failed to initialize potentials (Negative cycle detected)";
        if (trace) {
            trace->addStep(stepCounter++, "ERROR", "Negative Cycle Detected",
                           "Potential initialization failed because graph contains an unreachable negative cycle.");
        }
        return result;
    }

    std::vector<long long> dist(n);
    std::vector<int> parentNode(n);
    std::vector<int> parentEdgeIdx(n);

    // 2. Successive Shortest Path Loop
    while (true) {
        // If target flow is satisfied, stop augmenting
        if (targetFlow > 0.0 && result.maxFlow >= targetFlow - EPSILON) {
            result.terminationReason = "Target traffic flow satisfied";
            result.isOptimal = true;
            break;
        }

        // Find cheapest path using Dijkstra with Johnson potentials
        bool pathFound = findCheapestPath(graph, source, sink, potentials, dist, parentNode, parentEdgeIdx);
        if (!pathFound) {
            result.terminationReason = "No more augmenting paths with available capacity";
            result.isOptimal = true;
            break;
        }

        // Reconstruct path for tracing before augmentation
        std::vector<int> pathVertices;
        std::vector<std::string> pathEdgeLabels;
        std::vector<double> preResiduals;
        std::vector<long long> edgeReducedCosts;
        double bottleneck = std::numeric_limits<double>::max();
        if (targetFlow > 0.0) {
            bottleneck = targetFlow - result.maxFlow;
        }

        int curr = sink;
        while (curr != source) {
            int u = parentNode[curr];
            int eIdx = parentEdgeIdx[curr];
            const FlowEdge& fwdEdge = graph.getEdgesFrom(u)[eIdx];
            bottleneck = std::min(bottleneck, fwdEdge.getResidualCapacity());
            pathVertices.push_back(curr);
            pathEdgeLabels.push_back(fwdEdge.label.empty() ? (graph.getNodeInfo(u).name + " -> " + graph.getNodeInfo(curr).name) : fwdEdge.label);
            preResiduals.push_back(fwdEdge.getResidualCapacity());
            long long rc = fwdEdge.unitCost + potentials[u] - potentials[curr];
            if (rc < 0) rc = 0;
            edgeReducedCosts.push_back(rc);
            curr = u;
        }
        pathVertices.push_back(source);
        std::reverse(pathVertices.begin(), pathVertices.end());
        std::reverse(pathEdgeLabels.begin(), pathEdgeLabels.end());
        std::reverse(preResiduals.begin(), preResiduals.end());
        std::reverse(edgeReducedCosts.begin(), edgeReducedCosts.end());

        // Augment flow along shortest path
        double remainingDemand = (targetFlow > 0.0) ? (targetFlow - result.maxFlow) : -1.0;
        double flowPushed = sendTraffic(graph, source, sink, parentNode, parentEdgeIdx, remainingDemand);
        if (flowPushed <= EPSILON) {
            result.terminationReason = "Bottleneck capacity zero";
            break;
        }

        result.maxFlow += flowPushed;
        result.iterations++;

        if (trace) {
            std::ostringstream pathStr;
            std::vector<std::string> pathNodeNames;
            for (size_t i = 0; i < pathVertices.size(); ++i) {
                std::string nName = graph.getNodeInfo(pathVertices[i]).name;
                pathNodeNames.push_back(nName);
                pathStr << nName;
                if (i + 1 < pathVertices.size()) pathStr << " -> ";
            }

            std::ostringstream edgesStr;
            std::vector<EdgeUpdateDto> updates;
            for (size_t i = 0; i < pathEdgeLabels.size(); ++i) {
                int u = pathVertices[i];
                int v = pathVertices[i + 1];
                double preRes = preResiduals[i];
                double postRes = preRes - flowPushed;
                edgesStr << "[" << pathEdgeLabels[i] << ": res " << static_cast<long long>(preRes)
                         << " -> " << static_cast<long long>(postRes) << ", redCost=" << edgeReducedCosts[i] << "] ";

                EdgeUpdateDto fwdUp;
                fwdUp.fromId = u;
                fwdUp.toId = v;
                fwdUp.fromName = graph.getNodeInfo(u).name;
                fwdUp.toName = graph.getNodeInfo(v).name;
                fwdUp.prevFlow = 0.0;
                fwdUp.newFlow = flowPushed;
                fwdUp.prevResidual = preRes;
                fwdUp.newResidual = postRes;
                fwdUp.isReverse = false;
                fwdUp.reducedCost = edgeReducedCosts[i];
                fwdUp.label = pathEdgeLabels[i];
                updates.push_back(fwdUp);

                EdgeUpdateDto revUp;
                revUp.fromId = v;
                revUp.toId = u;
                revUp.fromName = graph.getNodeInfo(v).name;
                revUp.toName = graph.getNodeInfo(u).name;
                revUp.prevFlow = 0.0;
                revUp.newFlow = -flowPushed;
                revUp.prevResidual = 0.0;
                revUp.newResidual = flowPushed;
                revUp.isReverse = true;
                revUp.reducedCost = -edgeReducedCosts[i];
                revUp.label = pathEdgeLabels[i] + " [Residual]";
                updates.push_back(revUp);
            }

            std::vector<CandidatePathDto> candidates;
            CandidatePathDto sel;
            sel.pathString = pathStr.str();
            sel.nodeNames = pathNodeNames;
            sel.pathCost = static_cast<double>(dist[sink]) / static_cast<double>(costScalingFactor);
            sel.bottleneck = flowPushed;
            sel.status = "SELECTED";
            sel.reason = "Dijkstra optimal shortest augmenting path (Reduced Cost: " + std::to_string(dist[sink]) + ")";
            candidates.push_back(sel);

            std::map<std::string, std::string> stepDetails;
            stepDetails["augmenting_path"] = pathStr.str();
            stepDetails["bottleneck_capacity"] = std::to_string(static_cast<long long>(flowPushed)) + " req/s";
            stepDetails["path_reduced_distance"] = std::to_string(dist[sink]);
            stepDetails["residual_capacity_updates"] = edgesStr.str();
            stepDetails["cumulative_routed_flow"] = std::to_string(static_cast<long long>(result.maxFlow)) + " req/s";

            trace->addStep(stepCounter++, "FLOW_AUGMENT",
                           "Augmenting Path #" + std::to_string(result.iterations) + " Pushed (" + std::to_string(static_cast<long long>(flowPushed)) + " req/s)",
                           "Dijkstra identified cheapest augmenting path with reduced cost " + std::to_string(dist[sink]) +
                           ". Bottleneck flow of " + std::to_string(static_cast<long long>(flowPushed)) + " req/s augmented across residual edges.",
                           stepDetails, pathVertices, pathNodeNames, flowPushed, updates, candidates);
        }

        // Update vertex potentials to maintain c^pi(u, v) >= 0
        updatePotentials(potentials, dist);

        if (trace) {
            std::map<std::string, std::string> potDetails;
            potDetails["dist_sink"] = std::to_string(dist[sink]);
            potDetails["potentials_formula"] = "pi'(v) = pi(v) + dist(v)";
            trace->addStep(stepCounter++, "POTENTIAL_UPDATE",
                           "Johnson Dual Potentials Updated (Iteration #" + std::to_string(result.iterations) + ")",
                           "Updated vertex potentials pi'(v) using Dijkstra shortest distances to guarantee non-negative reduced edge costs c^pi(u,v) >= 0 for subsequent priority-queue searches.",
                           potDetails);
        }
    }

    // 3. Compute final costs from forward flow assignments
    for (int u = 0; u < n; ++u) {
        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (!edge.isReverse && edge.flow > EPSILON) {
                long long edgeScaledCost = static_cast<long long>(std::round(edge.flow * static_cast<double>(edge.unitCost)));
                result.totalScaledCost += edgeScaledCost;

                double edgeContinuousCost = edge.flow * (static_cast<double>(edge.unitCost) / static_cast<double>(costScalingFactor));
                result.totalContinuousCost += edgeContinuousCost;
            }
        }
    }

    if (trace) {
        std::map<std::string, std::string> termDetails;
        termDetails["termination_reason"] = result.terminationReason;
        termDetails["total_flow_delivered"] = std::to_string(static_cast<long long>(result.maxFlow)) + " req/s";
        termDetails["total_scaled_cost"] = std::to_string(result.totalScaledCost);
        std::ostringstream costSs;
        costSs << std::fixed << std::setprecision(2) << result.totalContinuousCost;
        termDetails["total_continuous_cost"] = "$" + costSs.str();
        termDetails["total_iterations"] = std::to_string(result.iterations);
        termDetails["is_optimal"] = result.isOptimal ? "true" : "false";

        trace->addStep(stepCounter++, "TERMINATION",
                       "MCMF Completed: " + result.terminationReason,
                       "Terminated successive augmentation loop with " + std::to_string(result.iterations) +
                       " path augmentations. Delivered " + std::to_string(static_cast<long long>(result.maxFlow)) +
                       " req/s at global minimum routing cost of $" + costSs.str() + ".",
                       termDetails);
    }

    return result;
}

// ==========================================
// Reporting & Extraction Helpers
// ==========================================
std::vector<FlowAllocation> MinCostMaxFlow::extractFlowAllocations(const Graph& graph, long long costScalingFactor) {
    std::vector<FlowAllocation> allocations;
    int n = graph.getNodeCount();

    for (int u = 0; u < n; ++u) {
        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (!edge.isReverse && edge.flow > EPSILON) {
                FlowAllocation alloc;
                alloc.fromNode = graph.getNodeInfo(u).name;
                alloc.toNode = graph.getNodeInfo(edge.to).name;
                alloc.flow = edge.flow;
                alloc.capacity = edge.capacity;
                alloc.unitCostContinuous = static_cast<double>(edge.unitCost) / static_cast<double>(costScalingFactor);
                alloc.totalCostContinuous = alloc.flow * alloc.unitCostContinuous;
                alloc.label = edge.label;
                allocations.push_back(alloc);
            }
        }
    }
    return allocations;
}

void MinCostMaxFlow::printFlowSummary(const Graph& graph, const MCMFResult& result, long long costScalingFactor) {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 MIN-COST MAX-FLOW (MCMF) ROUTING OPTIMIZATION RESULTS                  \n";
    std::cout << "========================================================================================\n";
    std::cout << "  Optimal Flow Status  : " << (result.isOptimal ? "[CONVERGED OPTIMAL]" : "[SUBOPTIMAL/INCOMPLETE]") << "\n";
    std::cout << "  Termination Reason   : " << result.terminationReason << "\n";
    std::cout << "  Augmenting Iterations: " << result.iterations << " path pushes\n";
    std::cout << "  Total Flow Routed    : " << std::fixed << std::setprecision(0) << result.maxFlow << " req/s\n";
    std::cout << "  Total Routing Cost   : " << std::fixed << std::setprecision(2) << result.totalContinuousCost 
              << " cost units (Scaled: " << result.totalScaledCost << ")\n\n";

    std::cout << "--- [Active Route Flow Allocations] ---\n";
    std::cout << "+------------------------------------------+--------------------+----------------+-----------------+\n";
    std::cout << "| Route Path                               | Flow (req/s)       | Unit Cost ($)  | Total Cost ($)  |\n";
    std::cout << "+------------------------------------------+--------------------+----------------+-----------------+\n";

    auto allocations = extractFlowAllocations(graph, costScalingFactor);
    for (const auto& a : allocations) {
        std::cout << "| " << std::left << std::setw(40) << a.label
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << a.flow
                  << " | " << std::setw(14) << std::fixed << std::setprecision(2) << a.unitCostContinuous
                  << " | " << std::setw(15) << std::fixed << std::setprecision(2) << a.totalCostContinuous << " |\n";
    }
    std::cout << "+------------------------------------------+--------------------+----------------+-----------------+\n";
    std::cout << "========================================================================================\n\n";
}

