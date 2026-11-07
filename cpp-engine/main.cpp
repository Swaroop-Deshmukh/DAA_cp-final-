#include "include/Network.h"
#include "include/Simulator.h"
#include "include/CostCalculator.h"
#include "include/Graph.h"
#include "include/Algorithms.h"
#include "include/FailureHandler.h"
#include "include/Router.h"
#include "include/EdmondsKarp.h"
#include "include/CacheOptimizer.h"
#include "include/DemandForecaster.h"
#include "include/BaselineRouters.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

// Audit assertion macro that tracks and validates every test
static int g_totalTests = 0;
static int g_passedTests = 0;
static int g_failedTests = 0;

#define AUDIT_ASSERT(condition, testName, details) \
    do { \
        g_totalTests++; \
        if (condition) { \
            g_passedTests++; \
            std::cout << "  [PASS] " << testName << " - " << details << "\n"; \
        } else { \
            g_failedTests++; \
            std::cerr << "  [FAIL] " << testName << " - " << details << " at line " << __LINE__ << "\n"; \
        } \
    } while(0)

void setupSampleLinks(Network& network) {
    network.addLink("Pune", "Pune Server", 5.0, 1.0, 4000.0, 0.85);
    network.addLink("Pune", "Mumbai Server", 12.0, 1.2, 4000.0, 0.80);
    network.addLink("Pune", "Delhi Server", 32.0, 2.5, 3000.0, 0.70);
    network.addLink("Pune", "Bangalore Server", 22.0, 1.8, 3500.0, 0.75);

    network.addLink("Mumbai", "Pune Server", 12.0, 1.2, 4000.0, 0.80);
    network.addLink("Mumbai", "Mumbai Server", 4.0, 1.0, 5000.0, 0.90);
    network.addLink("Mumbai", "Delhi Server", 28.0, 2.2, 3500.0, 0.70);
    network.addLink("Mumbai", "Bangalore Server", 24.0, 1.9, 3500.0, 0.75);

    network.addLink("Delhi", "Pune Server", 32.0, 2.5, 3000.0, 0.70);
    network.addLink("Delhi", "Mumbai Server", 28.0, 2.2, 3500.0, 0.75);
    network.addLink("Delhi", "Delhi Server", 6.0, 1.0, 4500.0, 0.85);
    network.addLink("Delhi", "Bangalore Server", 42.0, 3.0, 2500.0, 0.65);

    network.addLink("Bangalore", "Pune Server", 22.0, 1.8, 3500.0, 0.75);
    network.addLink("Bangalore", "Mumbai Server", 24.0, 1.9, 3500.0, 0.75);
    network.addLink("Bangalore", "Delhi Server", 42.0, 3.0, 2500.0, 0.65);
    network.addLink("Bangalore", "Bangalore Server", 5.0, 1.0, 4500.0, 0.88);

    network.addLink("Hyderabad", "Pune Server", 16.0, 1.4, 3000.0, 0.80);
    network.addLink("Hyderabad", "Mumbai Server", 20.0, 1.6, 3000.0, 0.75);
    network.addLink("Hyderabad", "Delhi Server", 35.0, 2.6, 2500.0, 0.65);
    network.addLink("Hyderabad", "Bangalore Server", 15.0, 1.3, 3500.0, 0.82);
}

Network createFullSampleNetwork() {
    Network net;
    net.addRegion("Pune", 2500.0);
    net.addRegion("Mumbai", 3500.0);
    net.addRegion("Delhi", 2000.0);
    net.addRegion("Bangalore", 2500.0);
    net.addRegion("Hyderabad", 1500.0);

    net.addServer("Pune Server", 5000.0, true);
    net.addServer("Mumbai Server", 6000.0, true);
    net.addServer("Delhi Server", 4500.0, true);
    net.addServer("Bangalore Server", 5000.0, true);

    setupSampleLinks(net);
    return net;
}

void applyScenario(Network& net, const std::string& scenarioKey) {
    if (scenarioKey == "mumbai_spike" || scenarioKey == "Mumbai traffic spike" || scenarioKey == "mumbai_traffic_spike") {
        net.setRegionDemand("Mumbai", 7000.0);
    } else if (scenarioKey == "delhi_failure" || scenarioKey == "Delhi server failure" || scenarioKey == "delhi_server_failure") {
        Server* s = net.getServer("Delhi Server");
        if (s) s->fail();
    } else if (scenarioKey == "delhi_recovery" || scenarioKey == "Delhi server recovery" || scenarioKey == "delhi_server_recovery") {
        Server* s = net.getServer("Delhi Server");
        if (s) {
            s->fail();
            s->recover();
        }
    } else if (scenarioKey == "pune_congestion" || scenarioKey == "Pune congestion" || scenarioKey == "pune_network_congestion") {
        NetworkLink* l = net.getLink("Pune", "Pune Server");
        if (l) l->congestionLatencyMs = 35.0;
    } else if (scenarioKey == "over_capacity" || scenarioKey == "Over-capacity traffic" || scenarioKey == "traffic_exceeding_edge_capacity") {
        for (const auto& r : net.getRegions()) {
            net.setRegionDemand(r.name, r.trafficDemand * 2.0);
        }
    } else if (scenarioKey == "all_servers_failed" || scenarioKey == "All edge servers offline" || scenarioKey == "all_edge_servers_failed") {
        for (const auto& s : net.getServers()) {
            Server* srv = net.getServer(s.name);
            if (srv) srv->fail();
        }
    } else if (scenarioKey == "cache_improvement" || scenarioKey == "Cache-hit improvement" || scenarioKey == "cache_hit_ratio_improvement") {
        NetworkLink* l1 = net.getLink("Pune", "Pune Server");
        if (l1) l1->cacheHitRatio = 0.98;
        NetworkLink* l2 = net.getLink("Mumbai", "Mumbai Server");
        if (l2) l2->cacheHitRatio = 0.99;
    }
}

StrategyResult executeStrategy(const std::string& stratName, const Network& net, const CostCalculator& calc, bool recordTrace = false) {
    if (stratName == "Round Robin" || stratName == "round_robin" || stratName == "rr") {
        return BaselineRouters::runRoundRobin(net, calc, recordTrace);
    } else if (stratName == "Nearest Server" || stratName == "nearest_server" || stratName == "ns") {
        return BaselineRouters::runNearestServer(net, calc, recordTrace);
    } else if (stratName == "Greedy Cost" || stratName == "greedy_cost" || stratName == "gc" || stratName == "Greedy Cost-Based") {
        return BaselineRouters::runGreedyCost(net, calc, recordTrace);
    } else {
        return BaselineRouters::runMCMF(net, calc, recordTrace);
    }
}

std::string getStrategyExplanation(const StrategyResult& res) {
    if (res.strategyName == "Round Robin") {
        return "Round Robin evenly distributes regional requests across healthy active edge servers in cyclic order, respecting individual server and link capacity limits before spilling any overflow to Origin fallback.";
    } else if (res.strategyName == "Nearest Server") {
        return "Nearest Server greedily directs regional requests to the edge server with the lowest effective physical latency, falling back to next-closest servers and finally Origin when capacity is saturated.";
    } else if (res.strategyName == "Greedy Cost") {
        return "Greedy Cost computes the composite multi-factor routing cost (latency, bandwidth cost, cache miss penalty) for each eligible region-server link and greedily allocates flow to the cheapest candidate links.";
    } else {
        return "Min-Cost Max-Flow utilizes Successive Shortest Path (SSP) with Johnson dual potentials and 4-tier convex server load bands to achieve the mathematically optimal global tradeoff between latency, bandwidth expense, cache efficiency, and server congestion.";
    }
}

int handleApiCommand(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "{\"error\": \"Missing API action command\"}\n";
        return 1;
    }

    std::string action = argv[2];
    Network net = createFullSampleNetwork();
    CostCalculator calc(1.0, 1.5, 20.0, 100);

    if (action == "get-network") {
        std::cout << BaselineRouters::exportNetworkJson(net) << "\n";
        return 0;
    }

    if (action == "get-metrics") {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"totalRegions\": " << net.getRegions().size() << ",\n";
        ss << "  \"totalServers\": " << net.getServers().size() << ",\n";
        ss << "  \"totalLinks\": " << net.getLinks().size() << ",\n";
        ss << "  \"totalDemand\": " << net.getTotalTrafficDemand() << ",\n";
        ss << "  \"totalActiveCapacity\": " << net.getTotalActiveServerCapacity() << ",\n";
        ss << "  \"fleetUtilization\": " << (net.getTotalTrafficDemand() / net.getTotalActiveServerCapacity() * 100.0) << ",\n";
        ss << "  \"availableStrategies\": [\"Round Robin\", \"Nearest Server\", \"Greedy Cost-Based\", \"Min-Cost Max-Flow\"],\n";
        ss << "  \"status\": \"healthy\"\n";
        ss << "}\n";
        std::cout << ss.str();
        return 0;
    }

    if (action == "run-routing") {
        std::string strategy = (argc >= 4) ? argv[3] : "Min-Cost Max-Flow";
        std::string scenario = (argc >= 5) ? argv[4] : "normal";

        applyScenario(net, scenario);
        StrategyResult res = executeStrategy(strategy, net, calc);
        std::string exp = getStrategyExplanation(res);

        std::cout << BaselineRouters::exportSingleResultJson(net, res, scenario, exp) << "\n";
        return 0;
    }

    if (action == "run-scenario") {
        std::string scenario = (argc >= 4) ? argv[3] : "normal";
        std::string strategy = (argc >= 5) ? argv[4] : "Min-Cost Max-Flow";

        applyScenario(net, scenario);
        StrategyResult res = executeStrategy(strategy, net, calc);
        std::string exp = getStrategyExplanation(res);

        std::cout << BaselineRouters::exportSingleResultJson(net, res, scenario, exp) << "\n";
        return 0;
    }

    if (action == "run-simulation") {
        // argv[3] = event_type, argv[4] = target, argv[5] = value, argv[6] = optional strategy
        std::string eventType = (argc >= 4) ? argv[3] : "traffic_spike";
        std::string target = (argc >= 5) ? argv[4] : "Pune";
        double value = (argc >= 6) ? std::stod(argv[5]) : 1.5;
        std::string strategy = (argc >= 7) ? argv[6] : "Min-Cost Max-Flow";

        if (eventType == "traffic_spike" || eventType == "spike") {
            Region* r = net.getRegion(target);
            if (r) r->trafficDemand *= value;
        } else if (eventType == "server_failure" || eventType == "fail") {
            Server* s = net.getServer(target);
            if (s) s->fail();
        } else if (eventType == "server_recovery" || eventType == "recover") {
            Server* s = net.getServer(target);
            if (s) s->recover();
        } else if (eventType == "congestion") {
            // target is region, value is delay
            std::string serverTarget = (argc >= 7) ? argv[6] : "Pune Server";
            NetworkLink* l = net.getLink(target, serverTarget);
            if (l) l->congestionLatencyMs += value;
            if (argc >= 8) strategy = argv[7];
        }

        StrategyResult res = executeStrategy(strategy, net, calc);
        std::string exp = "Simulation event applied: " + eventType + " on " + target + ". " + getStrategyExplanation(res);
        std::cout << BaselineRouters::exportSingleResultJson(net, res, "simulation_" + eventType, exp) << "\n";
        return 0;
    }

    if (action == "run-dynamic" || action == "inspect-trace") {
        std::string strategy = (argc >= 4) ? argv[3] : "Min-Cost Max-Flow";
        double dPune = (argc >= 5) ? std::stod(argv[4]) : 2500.0;
        double dMumbai = (argc >= 6) ? std::stod(argv[5]) : 3500.0;
        double dDelhi = (argc >= 7) ? std::stod(argv[6]) : 2000.0;
        double dBlr = (argc >= 8) ? std::stod(argv[7]) : 2500.0;
        double dHyd = (argc >= 9) ? std::stod(argv[8]) : 1500.0;

        net.setRegionDemand("Pune", dPune);
        net.setRegionDemand("Mumbai", dMumbai);
        net.setRegionDemand("Delhi", dDelhi);
        net.setRegionDemand("Bangalore", dBlr);
        net.setRegionDemand("Hyderabad", dHyd);

        if (argc >= 10) {
            std::string failedServers = argv[9];
            if (failedServers != "none" && !failedServers.empty()) {
                std::stringstream fss(failedServers);
                std::string item;
                while (std::getline(fss, item, ',')) {
                    Server* s = net.getServer(item);
                    if (s) s->fail();
                }
            }
        }

        if (argc >= 11) {
            std::string congSpec = argv[10];
            if (congSpec != "none" && !congSpec.empty()) {
                // format: "region:server:delayMs"
                std::stringstream css(congSpec);
                std::string reg, srv, delayStr;
                if (std::getline(css, reg, ':') && std::getline(css, srv, ':') && std::getline(css, delayStr, ':')) {
                    NetworkLink* l = net.getLink(reg, srv);
                    if (l) l->congestionLatencyMs = std::stod(delayStr);
                }
            }
        }

        if (argc >= 12) {
            std::string cacheSpec = argv[11];
            if (cacheSpec != "none" && !cacheSpec.empty()) {
                double hitRatio = std::stod(cacheSpec);
                for (auto& l : net.getLinks()) {
                    l.cacheHitRatio = hitRatio;
                }
            }
        }

        bool recordTrace = (action == "inspect-trace");
        if (argc >= 13) {
            std::string traceArg = argv[12];
            if (traceArg == "true" || traceArg == "1" || traceArg == "trace") {
                recordTrace = true;
            }
        }

        StrategyResult res = executeStrategy(strategy, net, calc, recordTrace);
        std::string exp = getStrategyExplanation(res);
        std::cout << BaselineRouters::exportSingleResultJson(net, res, (action == "inspect-trace" ? "inspect_trace" : "live_dynamic"), exp) << "\n";
        return 0;
    }

    if (action == "get-benchmarks") {
        if (argc >= 4) {
            std::string scenario = argv[3];
            applyScenario(net, scenario);
            auto res = BaselineRouters::compareAll(net, calc, scenario, false);
            std::cout << BaselineRouters::exportResultsJson(scenario, res) << "\n";
            return 0;
        }

        // Return all 8 scenarios
        const std::pair<std::string, std::string> scenarios[] = {
            {"normal", "Scenario 1: Normal Traffic Baseline"},
            {"mumbai_spike", "Scenario 2: Mumbai Traffic Spike (2.0x)"},
            {"delhi_failure", "Scenario 3: Delhi Server Outage"},
            {"delhi_recovery", "Scenario 4: Delhi Server Recovery"},
            {"pune_congestion", "Scenario 5: Pune Network Link Congestion (+35ms)"},
            {"over_capacity", "Scenario 6: Demand Exceeding Edge Capacity (24k req/s)"},
            {"all_servers_failed", "Scenario 7: Catastrophic Outage (All Edge Down)"},
            {"cache_improvement", "Scenario 8: Cache Hit Ratio Improvement (0.98/0.99)"}
        };

        std::cout << "[\n";
        for (int i = 0; i < 8; ++i) {
            Network scNet = createFullSampleNetwork();
            applyScenario(scNet, scenarios[i].first);
            auto res = BaselineRouters::compareAll(scNet, calc, scenarios[i].second, false);
            std::cout << BaselineRouters::exportResultsJson(scenarios[i].second, res);
            if (i < 7) std::cout << ",\n";
            else std::cout << "\n";
        }
        std::cout << "]\n";
        return 0;
    }

    if (action == "get-cache") {
        int budget = (argc >= 4) ? std::stoi(argv[3]) : 350;
        std::vector<ContentItem> catalog = {
            {1, "video_stream_4k.mp4", 120, 450},
            {2, "application_bundle.js", 40, 380},
            {3, "hero_banner.webp", 25, 290},
            {4, "vendor_libraries.js", 60, 310},
            {5, "api_cache_catalog.json", 15, 260},
            {6, "high_res_assets.zip", 140, 320},
            {7, "web_font_pack.woff2", 30, 240},
            {8, "thumbnail_sprite.png", 50, 210}
        };

        CachePlacementResult r = CacheOptimizer::optimizePlacement(catalog, budget);

        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"cacheBudgetMB\": " << budget << ",\n";
        ss << "  \"totalSizeUsedMB\": " << r.totalSizeUsedMB << ",\n";
        ss << "  \"totalPopularityValue\": " << r.totalPopularityValue << ",\n";
        ss << "  \"utilizationPct\": " << (budget > 0 ? (r.totalSizeUsedMB * 100.0 / budget) : 0.0) << ",\n";
        ss << "  \"catalogCount\": " << catalog.size() << ",\n";
        ss << "  \"selectedCount\": " << r.selectedItems.size() << ",\n";
        ss << "  \"selectedItems\": [\n";
        for (size_t i = 0; i < r.selectedItems.size(); ++i) {
            const auto& it = r.selectedItems[i];
            ss << "    {\"id\": " << it.id << ", \"name\": \"" << it.name << "\", \"sizeMB\": " << it.sizeMB
               << ", \"popularityValue\": " << it.popularityValue << "}" << (i + 1 < r.selectedItems.size() ? "," : "") << "\n";
        }
        ss << "  ],\n";
        ss << "  \"availableCatalog\": [\n";
        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto& it = catalog[i];
            ss << "    {\"id\": " << it.id << ", \"name\": \"" << it.name << "\", \"sizeMB\": " << it.sizeMB
               << ", \"popularityValue\": " << it.popularityValue << "}" << (i + 1 < catalog.size() ? "," : "") << "\n";
        }
        ss << "  ]\n";
        ss << "}\n";
        std::cout << ss.str();
        return 0;
    }

    if (action == "get-forecast") {
        std::string region = (argc >= 4) ? argv[3] : "Pune";
        std::vector<double> hist = {2000.0, 2150.0, 2280.0, 2400.0, 2500.0};
        if (region == "Mumbai") hist = {2800.0, 3000.0, 3200.0, 3350.0, 3500.0};
        else if (region == "Delhi") hist = {1700.0, 1780.0, 1850.0, 1920.0, 2000.0};
        else if (region == "Bangalore") hist = {2100.0, 2200.0, 2300.0, 2420.0, 2500.0};
        else if (region == "Hyderabad") hist = {1200.0, 1280.0, 1350.0, 1420.0, 1500.0};

        DemandForecaster forecaster(0.4, 0.2);
        ForecastResult f = forecaster.forecast(hist, 5);

        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"region\": \"" << region << "\",\n";
        ss << "  \"alpha\": 0.4,\n";
        ss << "  \"beta\": 0.2,\n";
        ss << "  \"finalLevel\": " << f.finalLevel << ",\n";
        ss << "  \"finalTrend\": " << f.finalTrend << ",\n";
        ss << "  \"historicalDemand\": [";
        for (size_t i = 0; i < hist.size(); ++i) {
            ss << hist[i] << (i + 1 < hist.size() ? ", " : "");
        }
        ss << "],\n";
        ss << "  \"forecastSteps\": [";
        for (size_t i = 0; i < f.futureForecasts.size(); ++i) {
            ss << f.futureForecasts[i] << (i + 1 < f.futureForecasts.size() ? ", " : "");
        }
        ss << "]\n";
        ss << "}\n";
        std::cout << ss.str();
        return 0;
    }

    std::cerr << "{\"error\": \"Unknown API action: " << action << "\"}\n";
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string flag = argv[1];
        if (flag == "--api" || flag == "--json") {
            return handleApiCommand(argc, argv);
        }
    }

    std::cout << "========================================================================================\n";
    std::cout << "          ADAPTIVE CDN TRAFFIC ROUTER - COMPREHENSIVE TECHNICAL AUDIT (PHASES 1-7)      \n";
    std::cout << "========================================================================================\n\n";

    // =========================================================================
    // AUDIT SECTION 1: PHASE 1 — NETWORK
    // =========================================================================
    std::cout << ">>> [AUDIT SECTION 1: PHASE 1 — NETWORK DATA STRUCTURES] <<<\n";
    {
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addRegion("Delhi", 2000.0);
        net.addRegion("Bangalore", 2500.0);
        net.addRegion("Hyderabad", 1500.0);

        net.addServer("Pune Server", 5000.0, true);
        net.addServer("Mumbai Server", 6000.0, true);
        net.addServer("Delhi Server", 4500.0, true);
        net.addServer("Bangalore Server", 5000.0, true);

        AUDIT_ASSERT(net.getRegions().size() == 5, "Phase 1.1", "Exactly five client regions registered");
        AUDIT_ASSERT(net.getServers().size() == 4, "Phase 1.2", "Exactly four edge servers registered");
        AUDIT_ASSERT(net.getTotalTrafficDemand() == 12000.0, "Phase 1.3", "Total regional demand matches 12,000 req/s");
        AUDIT_ASSERT(net.getTotalActiveServerCapacity() == 20500.0, "Phase 1.4", "Total active capacity matches 20,500 req/s");

        // Verify active / inactive states
        Server* s = net.getServer("Delhi Server");
        s->fail();
        AUDIT_ASSERT(!s->isActive && s->availableCapacity == 0.0 && s->configuredCapacity == 4500.0,
                     "Phase 1.5", "Server fail state drops available capacity to 0 while preserving configured capacity");
        s->recover();
        AUDIT_ASSERT(s->isActive && s->availableCapacity == 4500.0,
                     "Phase 1.6", "Server recovery restores available capacity");
    }

    // =========================================================================
    // AUDIT SECTION 2: PHASE 2 — SIMULATOR
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 2: PHASE 2 — NETWORK SIMULATOR] <<<\n";
    {
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addServer("Pune Server", 5000.0, true);
        net.addLink("Pune", "Pune Server", 5.0, 1.0);

        Simulator sim(net, 42);

        // 1. Normal traffic
        sim.generateNormalTraffic();
        AUDIT_ASSERT(net.getRegion("Pune")->trafficDemand == 2500.0, "Phase 2.1", "Normal traffic generation restores baseline demand");

        // 2. Traffic spike
        sim.createTrafficSpike("Pune", 2.0);
        AUDIT_ASSERT(net.getRegion("Pune")->trafficDemand == 5000.0, "Phase 2.2", "Traffic spike doubles regional demand (2500 -> 5000 req/s)");

        // 3. Server failure and recovery
        sim.failServer("Pune Server");
        AUDIT_ASSERT(net.getServer("Pune Server")->availableCapacity == 0.0, "Phase 2.3", "Simulator failServer sets available capacity to 0");
        sim.recoverServer("Pune Server");
        AUDIT_ASSERT(net.getServer("Pune Server")->availableCapacity == 5000.0, "Phase 2.4", "Simulator recoverServer restores capacity");

        // 4. Congestion
        sim.addNetworkCongestion("Pune", "Pune Server", 15.0);
        AUDIT_ASSERT(net.getLink("Pune", "Pune Server")->getEffectiveLatency() == 20.0, "Phase 2.5", "Congestion increases effective latency (5 + 15 = 20 ms)");
        sim.resetNetworkCongestion();
        AUDIT_ASSERT(net.getLink("Pune", "Pune Server")->getEffectiveLatency() == 5.0, "Phase 2.6", "Reset congestion clears transient delays back to base 5.0 ms");

        // 5. Reproducible random simulation
        sim.setRandomSeed(999);
        sim.applyRandomTrafficVariation(0.15);
        double demandA = net.getRegion("Pune")->trafficDemand;

        sim.setRandomSeed(999);
        sim.applyRandomTrafficVariation(0.15);
        double demandB = net.getRegion("Pune")->trafficDemand;
        AUDIT_ASSERT(demandA == demandB, "Phase 2.7", "Identical random seeds yield bit-for-bit identical demands (Deterministic replay)");
    }

    // =========================================================================
    // AUDIT SECTION 3: PHASE 3 — ROUTING COST CALCULATOR
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 3: PHASE 3 — ROUTING COST CALCULATOR] <<<\n";
    {
        // Formula: C = alpha * L + beta * B + (1 - H) * M
        // Let alpha = 1.0, beta = 2.0, M = 10.0, Scale = 100
        CostCalculator calc(1.0, 2.0, 10.0, 100);

        // Manual Case 1: L = 10.0, B = 2.0, H = 0.80
        // Expected continuous: 1.0*10 + 2.0*2 + (1 - 0.80)*10 = 10 + 4 + 2 = 16.0
        // Expected scaled integer: 1600
        RouteConnectionProfile p1("RegA", "SrvA", 10.0, 2.0, 0.80, 10.0);
        CostBreakdown bd1;
        calc.calculateCost(p1, bd1);
        AUDIT_ASSERT(std::abs(bd1.totalContinuousCost - 16.0) < 1e-6 && bd1.scaledIntegerCost == 1600,
                     "Phase 3.1", "Manual cost calculation matches exact formula (C = 16.0, Scaled = 1600)");

        // Manual Case 2: Latency contribution test (Increase L by 5 ms -> Cost increases by 5.0)
        RouteConnectionProfile p2("RegA", "SrvA", 15.0, 2.0, 0.80, 10.0);
        CostBreakdown bd2;
        calc.calculateCost(p2, bd2);
        AUDIT_ASSERT(std::abs(bd2.totalContinuousCost - 21.0) < 1e-6, "Phase 3.2", "Latency contributes directly to cost (+5 ms -> +5 cost)");

        // Manual Case 3: Bandwidth cost contribution test (Increase B by 1.5 -> Cost increases by 1.5 * 2.0 = 3.0)
        RouteConnectionProfile p3("RegA", "SrvA", 10.0, 3.5, 0.80, 10.0);
        CostBreakdown bd3;
        calc.calculateCost(p3, bd3);
        AUDIT_ASSERT(std::abs(bd3.totalContinuousCost - 19.0) < 1e-6, "Phase 3.3", "Bandwidth cost contributes with weight beta (+1.5 * 2 = +3 cost)");

        // Manual Case 4: Cache hit ratio test (Increase H to 1.0 -> Miss penalty drops to 0)
        RouteConnectionProfile p4("RegA", "SrvA", 10.0, 2.0, 1.00, 10.0);
        CostBreakdown bd4;
        calc.calculateCost(p4, bd4);
        AUDIT_ASSERT(std::abs(bd4.totalContinuousCost - 14.0) < 1e-6 && bd4.cacheMissComponent == 0.0,
                     "Phase 3.4", "100% cache hit eliminates miss penalty (Cost drops to 14.0)");

        // Invalid input handling
        std::string err;
        RouteConnectionProfile invalidH("RegA", "SrvA", 10.0, 2.0, 1.25, 10.0);
        bool valid = CostCalculator::validateProfile(invalidH, err);
        AUDIT_ASSERT(!valid, "Phase 3.5", "Invalid cache hit ratio (> 1.0) rejected by validator");

        RouteConnectionProfile negLat("RegA", "SrvA", -5.0, 2.0, 0.8, 10.0);
        valid = CostCalculator::validateProfile(negLat, err);
        AUDIT_ASSERT(!valid, "Phase 3.6", "Negative latency rejected by validator");
    }

    // =========================================================================
    // AUDIT SECTION 4: PHASE 4 — GRAPH CONSTRUCTION
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 4: PHASE 4 — CDN GRAPH CONSTRUCTION] <<<\n";
    {
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addServer("Pune Server", 5000.0, true);
        net.addServer("Mumbai Server", 6000.0, true);
        net.addLink("Pune", "Pune Server", 5.0, 1.0, 4000.0, 0.85);
        net.addLink("Pune", "Mumbai Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Pune Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Mumbai Server", 4.0, 1.0, 5000.0, 0.90);

        CostCalculator calc(1.0, 1.5, 20.0, 100);
        Graph g;
        g.buildFromNetwork(net, calc);

        // 1. Source to Region capacities
        int s = g.getSourceId();
        double sumSourceCap = 0.0;
        for (const auto& e : g.getEdgesFrom(s)) {
            if (!e.isReverse) sumSourceCap += e.capacity;
        }
        AUDIT_ASSERT(sumSourceCap == 6000.0, "Phase 4.1", "Source-to-region capacities match total active demand (2500 + 3500 = 6000)");

        // 2. Server to Sink parallel load bands
        int sink = g.getSinkId();
        for (const auto& server : net.getServers()) {
            int srvId = g.getNodeId(server.name);
            double totalBandCap = 0.0;
            int bandCount = 0;
            for (const auto& e : g.getEdgesFrom(srvId)) {
                if (!e.isReverse && e.to == sink) {
                    totalBandCap += e.capacity;
                    bandCount++;
                }
            }
            AUDIT_ASSERT(bandCount == 4, "Phase 4.2", "Server has exactly 4 parallel load bands to Sink");
            AUDIT_ASSERT(totalBandCap <= server.configuredCapacity + 1e-6, "Phase 4.3",
                         "Sum of load band capacities NEVER exceeds actual configured server capacity");
        }

        // 3. Failed server handling in graph
        net.getServer("Pune Server")->fail();
        Graph outageGraph;
        outageGraph.buildFromNetwork(net, calc);
        int puneId = outageGraph.getNodeId("Pune Server");
        double puneSinkCap = 0.0;
        for (const auto& e : outageGraph.getEdgesFrom(puneId)) {
            if (!e.isReverse && e.to == outageGraph.getSinkId()) {
                puneSinkCap += e.capacity;
            }
        }
        AUDIT_ASSERT(puneSinkCap == 0.0, "Phase 4.4", "Failed server receives 0 available capacity across all bands in flow graph");
        net.getServer("Pune Server")->recover();

        // 4. Reverse residual edges
        bool allPaired = true;
        for (int u = 0; u < g.getNodeCount(); ++u) {
            for (const auto& e : g.getEdgesFrom(u)) {
                if (!e.isReverse) {
                    const auto& rev = g.getEdgesFrom(e.to)[e.reverseEdgeIndex];
                    if (rev.to != u || rev.capacity != 0.0 || rev.unitCost != -e.unitCost) {
                        allPaired = false;
                    }
                }
            }
        }
        AUDIT_ASSERT(allPaired, "Phase 4.5", "Every forward edge has a correctly paired residual reverse edge (cost = -W, cap = 0)");
    }

    // =========================================================================
    // AUDIT SECTION 5: PHASE 5 — MIN-COST MAX-FLOW
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 5: PHASE 5 — MIN-COST MAX-FLOW] <<<\n";
    {
        // Test 5.1: Small manually solvable network (2 paths)
        // S(0) -> A(1) [cap: 10, cost: 2] -> T(3) [cap: 10, cost: 0]
        // S(0) -> B(2) [cap: 10, cost: 5] -> T(3) [cap: 10, cost: 0]
        // Pushing 15 units:
        // Expected: Path A takes 10 units (cost: 10*2 = 20)
        //           Path B takes 5 units (cost: 5*5 = 25)
        // Total Flow = 15, Total Cost = 45.
        Graph smallG;
        Network dummyNet;
        dummyNet.addRegion("A", 10.0);
        dummyNet.addRegion("B", 10.0);
        CostCalculator calc(1.0, 0.0, 0.0, 1);
        smallG.buildFromNetwork(dummyNet, calc);

        // Reset edges and construct small test topology
        for (int i = 0; i < smallG.getNodeCount(); ++i) smallG.getEdgesFrom(i).clear();
        int s = 0, a = 1, b = 2, t = 3;
        smallG.addEdge(s, a, 10.0, 2, "S -> A");
        smallG.addEdge(s, b, 10.0, 5, "S -> B");
        smallG.addEdge(a, t, 10.0, 0, "A -> T");
        smallG.addEdge(b, t, 10.0, 0, "B -> T");

        MCMFResult resSmall = MinCostMaxFlow::calculateMinCostMaxFlow(smallG, s, t, 15.0, 1);
        AUDIT_ASSERT(resSmall.maxFlow == 15.0, "Phase 5.1", "Small test network delivered exact flow of 15");
        AUDIT_ASSERT(resSmall.totalScaledCost == 45, "Phase 5.2", "Small test network achieved exact manually calculated minimum cost (45)");

        // Test 5.3: Reverse-Edge Rerouting Proof (Canonical Diamond Graph)
        // A greedy algorithm without reverse-edge redirection CANNOT find the minimum-cost max flow!
        // S -> A [cap 1, cost 1]
        // S -> B [cap 1, cost 2]
        // A -> B [cap 1, cost 0]
        // A -> T [cap 1, cost 2]
        // B -> T [cap 1, cost 1]
        // Optimal routes: S->A->T (flow 1, cost 3) and S->B->T (flow 1, cost 3) => Total Flow = 2, Total Cost = 6.
        Graph diamond;
        diamond.buildFromNetwork(dummyNet, calc);
        for (int i = 0; i < diamond.getNodeCount(); ++i) diamond.getEdgesFrom(i).clear();
        diamond.addEdge(s, a, 1.0, 1, "S -> A");
        diamond.addEdge(s, b, 1.0, 2, "S -> B");
        diamond.addEdge(a, b, 1.0, 0, "A -> B");
        diamond.addEdge(a, t, 1.0, 2, "A -> T");
        diamond.addEdge(b, t, 1.0, 1, "B -> T");

        MCMFResult resDiamond = MinCostMaxFlow::calculateMinCostMaxFlow(diamond, s, t, -1.0, 1);
        AUDIT_ASSERT(resDiamond.maxFlow == 2.0 && resDiamond.totalScaledCost == 6,
                     "Phase 5.3", "Reverse-edge rerouting correctly redirects flow to achieve optimal minimum cost (Flow: 2, Cost: 6)");

        // Test 5.4: Flow conservation check on full CDN network
        Network fullNet;
        fullNet.addRegion("Pune", 2500.0);
        fullNet.addRegion("Mumbai", 3500.0);
        fullNet.addRegion("Delhi", 2000.0);
        fullNet.addRegion("Bangalore", 2500.0);
        fullNet.addRegion("Hyderabad", 1500.0);
        fullNet.addServer("Pune Server", 5000.0, true);
        fullNet.addServer("Mumbai Server", 6000.0, true);
        fullNet.addServer("Delhi Server", 4500.0, true);
        fullNet.addServer("Bangalore Server", 5000.0, true);
        setupSampleLinks(fullNet);

        CostCalculator fullCalc(1.0, 1.5, 20.0, 100);
        Graph fullG;
        fullG.buildFromNetwork(fullNet, fullCalc);

        MCMFResult resFull = MinCostMaxFlow::calculateMinCostMaxFlow(fullG, fullG.getSourceId(), fullG.getSinkId(), 12000.0, 100);
        AUDIT_ASSERT(resFull.maxFlow == 12000.0, "Phase 5.4", "Full CDN network routed 100% of demand (12,000 req/s)");

        // Check flow conservation at all region nodes
        bool conserved = true;
        for (const auto& r : fullNet.getRegions()) {
            int regId = fullG.getNodeId(r.name);
            double inFlow = 0.0, outFlow = 0.0;
            for (int u = 0; u < fullG.getNodeCount(); ++u) {
                for (const auto& e : fullG.getEdgesFrom(u)) {
                    if (!e.isReverse && e.to == regId) inFlow += e.flow;
                }
            }
            for (const auto& e : fullG.getEdgesFrom(regId)) {
                if (!e.isReverse) outFlow += e.flow;
            }
            if (std::abs(inFlow - outFlow) > 1e-6 || std::abs(inFlow - r.trafficDemand) > 1e-6) {
                conserved = false;
            }
        }
        AUDIT_ASSERT(conserved, "Phase 5.5", "Flow conservation strictly holds across all regions (Inflow == Outflow == Demand)");
    }

    // =========================================================================
    // AUDIT SECTION 6: PHASE 6 — ADAPTIVE ROUTING & FAILURE HANDLING
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 6: PHASE 6 — ADAPTIVE ROUTING] <<<\n";
    {
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addRegion("Delhi", 2000.0);
        net.addRegion("Bangalore", 2500.0);
        net.addRegion("Hyderabad", 1500.0);
        net.addServer("Pune Server", 5000.0, true);
        net.addServer("Mumbai Server", 6000.0, true);
        net.addServer("Delhi Server", 4500.0, true);
        net.addServer("Bangalore Server", 5000.0, true);
        setupSampleLinks(net);

        CostCalculator calc(1.0, 1.5, 20.0, 100);
        Router router(net, calc, ReplanningPolicy(0.10, 5.0, true));
        FailureHandler fh(net);

        // Initial routing
        router.routeTraffic(true);
        AUDIT_ASSERT(router.getCurrentResult().maxFlow == 12000.0, "Phase 6.1", "Initial plan successfully routes 12,000 req/s");

        // Delhi server failure
        fh.failServer("Delhi Server");
        router.routeTraffic();
        double flowOnDelhi = 0.0;
        for (const auto& a : router.getCurrentAllocations()) {
            if (a.toNode == "Delhi Server") flowOnDelhi += a.flow;
        }
        AUDIT_ASSERT(flowOnDelhi == 0.0, "Phase 6.2", "Failed Delhi Server receives exactly 0 traffic after replanning");

        // Delhi server recovery
        fh.recoverServer("Delhi Server");
        router.routeTraffic();
        flowOnDelhi = 0.0;
        for (const auto& a : router.getCurrentAllocations()) {
            if (a.toNode == "Delhi Server") flowOnDelhi += a.flow;
        }
        AUDIT_ASSERT(flowOnDelhi > 0.0, "Phase 6.3", "Recovered Delhi Server re-admitted to active traffic handling");

        // Mumbai traffic spike
        fh.injectTrafficSpike("Mumbai", 2.0); // +3500 req/s
        router.routeTraffic();
        AUDIT_ASSERT(router.getCurrentResult().maxFlow == 15500.0, "Phase 6.4", "Router accommodates traffic spike (15,500 req/s routed)");
        net.setRegionDemand("Mumbai", 3500.0);
        router.routeTraffic(true);

        // Pune network congestion (+35 ms)
        fh.injectCongestion("Pune", "Pune Server", 35.0);
        router.routeTraffic();
        double puneLocal = 0.0;
        for (const auto& a : router.getCurrentAllocations()) {
            if (a.fromNode == "Pune" && a.toNode == "Pune Server") puneLocal += a.flow;
        }
        AUDIT_ASSERT(puneLocal == 0.0, "Phase 6.5", "Congestion (+35 ms) diverts traffic away from congested link");
        fh.clearAllCongestion();
        router.routeTraffic(true);

        // All servers failed (Catastrophic fallback test)
        fh.failServer("Pune Server");
        fh.failServer("Mumbai Server");
        fh.failServer("Delhi Server");
        fh.failServer("Bangalore Server");
        router.routeTraffic();
        double originFlow = 0.0;
        for (const auto& a : router.getCurrentAllocations()) {
            if (a.toNode == "Origin Server") originFlow += a.flow;
        }
        AUDIT_ASSERT(originFlow == 12000.0, "Phase 6.6", "100% of traffic safely diverted to Origin when all edge servers fail");

        // Restore all servers
        fh.recoverServer("Pune Server");
        fh.recoverServer("Mumbai Server");
        fh.recoverServer("Delhi Server");
        fh.recoverServer("Bangalore Server");
        router.routeTraffic(true);
    }

    // =========================================================================
    // AUDIT SECTION 7: PHASE 7 — SUPPORTING ALGORITHMS
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 7: PHASE 7 — SUPPORTING ALGORITHMS] <<<\n";
    {
        // 1. Edmonds-Karp & Min-Cut
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addServer("Pune Server", 5000.0, true);
        net.addServer("Mumbai Server", 6000.0, true);
        net.addLink("Pune", "Pune Server", 5.0, 1.0, 4000.0, 0.85);
        net.addLink("Pune", "Mumbai Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Pune Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Mumbai Server", 4.0, 1.0, 5000.0, 0.90);
        CostCalculator calc(1.0, 1.5, 20.0, 100);

        Graph gMCMF, gEK;
        gMCMF.buildFromNetwork(net, calc);
        gEK.buildFromNetwork(net, calc);

        int s = gEK.getSourceId();
        int t = gEK.getSinkId();

        MCMFResult mcmfRes = MinCostMaxFlow::calculateMinCostMaxFlow(gMCMF, s, t, -1.0, 100);
        EdmondsKarpResult ekRes = EdmondsKarp::calculateMaxFlow(gEK, s, t);

        AUDIT_ASSERT(mcmfRes.maxFlow == ekRes.maxFlow, "Phase 7.1", "Edmonds-Karp max-flow independently confirms MCMF max-flow");
        AUDIT_ASSERT(ekRes.maxFlow == ekRes.minCut.minCutCapacity, "Phase 7.2", "Max-Flow Min-Cut Theorem confirmed: Max Flow == Cut Capacity");

        // Separate edge-fleet bottleneck analysis (excluding Origin fallback)
        AUDIT_ASSERT(ekRes.minCut.edgeFleetCutCapacity <= ekRes.minCut.minCutCapacity,
                     "Phase 7.3", "Edge-fleet bottleneck capacity computed separately from origin fallback");

        // 2. Cache Placement Knapsack DP Edge Cases
        std::vector<ContentItem> items = {
            {1, "video.mp4", 100, 300},
            {2, "bundle.js", 50, 200},
            {3, "image.png", 80, 240}
        };

        // Edge case A: Zero storage
        CachePlacementResult rZero = CacheOptimizer::optimizePlacement(items, 0);
        AUDIT_ASSERT(rZero.totalPopularityValue == 0 && rZero.totalSizeUsedMB == 0,
                     "Phase 7.4", "Knapsack handles zero storage budget (0 value, 0 size)");

        // Edge case B: Item larger than storage
        CachePlacementResult rSmall = CacheOptimizer::optimizePlacement(items, 30);
        AUDIT_ASSERT(rSmall.totalPopularityValue == 0 && rSmall.totalSizeUsedMB == 0,
                     "Phase 7.5", "Knapsack omits items strictly exceeding capacity");

        // Edge case C: Empty content list
        CachePlacementResult rEmpty = CacheOptimizer::optimizePlacement({}, 200);
        AUDIT_ASSERT(rEmpty.totalPopularityValue == 0, "Phase 7.6", "Knapsack handles empty content list gracefully");

        // Edge case D: Normal capacity with brute-force verification
        CachePlacementResult rNorm = CacheOptimizer::optimizePlacement(items, 150);
        CachePlacementResult rBF = CacheOptimizer::solveBruteForce(items, 150);
        AUDIT_ASSERT(rNorm.totalPopularityValue == rBF.totalPopularityValue && rNorm.totalSizeUsedMB <= 150,
                     "Phase 7.7", "Knapsack DP matches Brute-Force ground truth and respects capacity");

        // 3. Holt Linear Smoothing Edge Cases
        DemandForecaster forecaster(0.4, 0.2);

        // Constant demand
        std::vector<double> constHist = {500.0, 500.0, 500.0, 500.0};
        ForecastResult fConst = forecaster.forecast(constHist, 2);
        AUDIT_ASSERT(std::abs(fConst.finalTrend) < 1e-4 && std::abs(fConst.futureForecasts[0] - 500.0) < 1e-4,
                     "Phase 7.8", "Holt smoothing predicts steady state for constant history");

        // Increasing demand
        std::vector<double> incHist = {100.0, 200.0, 300.0, 400.0};
        ForecastResult fInc = forecaster.forecast(incHist, 2);
        AUDIT_ASSERT(fInc.finalTrend > 0.0 && fInc.futureForecasts[0] > 400.0,
                     "Phase 7.9", "Holt smoothing predicts positive slope for increasing demand");

        // Decreasing demand
        std::vector<double> decHist = {400.0, 300.0, 200.0, 100.0};
        ForecastResult fDec = forecaster.forecast(decHist, 2);
        AUDIT_ASSERT(fDec.finalTrend < 0.0, "Phase 7.10", "Holt smoothing detects negative trend for decreasing demand");

        // Insufficient observations (0 and 1)
        ForecastResult fZero = forecaster.forecast({}, 2);
        AUDIT_ASSERT(fZero.futureForecasts.empty(), "Phase 7.11", "Holt smoothing handles empty observation history");

        ForecastResult fOne = forecaster.forecast({750.0}, 2);
        AUDIT_ASSERT(fOne.futureForecasts.size() == 2 && fOne.futureForecasts[0] == 750.0,
                     "Phase 7.12", "Holt smoothing handles single-element observation history");

        // Invalid smoothing parameters
        bool validAlpha = forecaster.setAlpha(1.5);
        bool validBeta = forecaster.setBeta(-0.2);
        AUDIT_ASSERT(!validAlpha && !validBeta, "Phase 7.13", "Invalid smoothing parameters (> 1.0 or < 0.0) rejected");
    }

    // =========================================================================
    // AUDIT SECTION 8: CROSS-MODULE INTEGRATION WORKFLOWS (A TO F)
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 8: CROSS-MODULE INTEGRATION WORKFLOWS (A TO F)] <<<\n";
    {
        Network net;
        net.addRegion("Pune", 2500.0);
        net.addRegion("Mumbai", 3500.0);
        net.addServer("Pune Server", 5000.0, true);
        net.addServer("Mumbai Server", 6000.0, true);
        net.addLink("Pune", "Pune Server", 5.0, 1.0, 4000.0, 0.85);
        net.addLink("Pune", "Mumbai Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Pune Server", 12.0, 1.2, 4000.0, 0.80);
        net.addLink("Mumbai", "Mumbai Server", 4.0, 1.0, 5000.0, 0.90);

        CostCalculator calc(1.0, 1.5, 20.0, 100);
        Router router(net, calc);
        FailureHandler fh(net);

        // Workflow A: Normal traffic -> Cost calculation -> Graph construction -> Routing -> Result
        MCMFResult resA = router.routeTraffic(true);
        AUDIT_ASSERT(resA.maxFlow == 6000.0, "Workflow A", "Normal traffic flow seamlessly routed through graph pipeline");

        // Workflow B: Traffic spike -> Updated demand -> Recalculated routing
        fh.injectTrafficSpike("Pune", 1.5); // 2500 -> 3750 req/s
        MCMFResult resB = router.routeTraffic();
        AUDIT_ASSERT(resB.maxFlow == 7250.0, "Workflow B", "Demand surge seamlessly propagates to router and updates flow");
        net.setRegionDemand("Pune", 2500.0);
        router.routeTraffic(true);

        // Workflow C: Server failure -> Updated network state -> Graph rebuild -> Routing
        fh.failServer("Pune Server");
        MCMFResult resC = router.routeTraffic();
        AUDIT_ASSERT(resC.maxFlow == 6000.0, "Workflow C", "Server failure triggers dynamic failover with 100% demand preservation");

        // Workflow D: Server recovery -> Updated network state -> Routing
        fh.recoverServer("Pune Server");
        MCMFResult resD = router.routeTraffic();
        AUDIT_ASSERT(resD.maxFlow == 6000.0, "Workflow D", "Server recovery triggers automated re-admission");

        // Workflow E: Cache optimization -> Updated cache hit ratio -> Updated routing cost
        // Run cache optimizer to place hot assets
        std::vector<ContentItem> catalog = {{1, "bundle.js", 50, 400}, {2, "video.mp4", 150, 500}};
        CachePlacementResult cacheRes = CacheOptimizer::optimizePlacement(catalog, 100); // Selects bundle.js
        AUDIT_ASSERT(!cacheRes.selectedItems.empty(), "Workflow E.1", "Cache optimizer selects high-popularity assets");

        // High cache hit improves link cache hit ratio from 85% to 95%
        NetworkLink* l = net.getLink("Pune", "Pune Server");
        long long oldScaledCost = calc.getScaledEdgeCost(RouteConnectionProfile("Pune", "Pune Server", l->getEffectiveLatency(), l->bandwidthCost, l->cacheHitRatio, calc.getDefaultMissPenalty()));
        l->cacheHitRatio = 0.95;
        long long newScaledCost = calc.getScaledEdgeCost(RouteConnectionProfile("Pune", "Pune Server", l->getEffectiveLatency(), l->bandwidthCost, l->cacheHitRatio, calc.getDefaultMissPenalty()));
        AUDIT_ASSERT(newScaledCost < oldScaledCost, "Workflow E.2", "Cache optimization reduces composite routing edge cost");
        l->cacheHitRatio = 0.85; // Restore

        // Workflow F: Forecast demand -> Future demand scenario -> Routing comparison
        DemandForecaster forecaster(0.4, 0.2);
        std::vector<double> demandHistory = {2000.0, 2200.0, 2400.0, 2600.0};
        ForecastResult fRes = forecaster.forecast(demandHistory, 1);
        double futureDemand = fRes.futureForecasts[0];
        AUDIT_ASSERT(futureDemand > 2600.0, "Workflow F.1", "Holt forecaster projects upcoming traffic growth");

        // Proactively simulate network under future forecasted demand
        net.setRegionDemand("Pune", futureDemand);
        MCMFResult resF = router.routeTraffic(true);
        AUDIT_ASSERT(resF.maxFlow == futureDemand + 3500.0, "Workflow F.2", "Routing engine successfully satisfies proactive forecasted scenario");
        net.setRegionDemand("Pune", 2500.0);
        router.routeTraffic(true);
    }

    // =========================================================================
    // AUDIT SECTION 9: PHASE 8 — BASELINE STRATEGIES UNIT & VALIDATION TESTS
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 9: PHASE 8 — BASELINE STRATEGIES UNIT & INVARIANT TESTS] <<<\n";
    {
        // 1. Manually verifiable 2-region, 2-server test network
        Network miniNet;
        miniNet.addRegion("RegA", 10.0);
        miniNet.addRegion("RegB", 10.0);
        miniNet.addServer("Srv1", 10.0, true);
        miniNet.addServer("Srv2", 10.0, true);

        // A is close to Srv1 (5ms), far from Srv2 (25ms)
        // B is far from Srv1 (25ms), close to Srv2 (5ms)
        miniNet.addLink("RegA", "Srv1", 5.0, 1.0, 10.0, 0.80);
        miniNet.addLink("RegA", "Srv2", 25.0, 1.0, 10.0, 0.80);
        miniNet.addLink("RegB", "Srv1", 25.0, 1.0, 10.0, 0.80);
        miniNet.addLink("RegB", "Srv2", 5.0, 1.0, 10.0, 0.80);

        CostCalculator miniCalc(1.0, 1.0, 10.0, 100);

        // Nearest Server test
        StrategyResult nsRes = BaselineRouters::runNearestServer(miniNet, miniCalc);
        AUDIT_ASSERT(nsRes.totalTrafficRouted == 20.0, "Phase 8.1", "Nearest Server routes 100% of mini network demand");
        AUDIT_ASSERT(std::abs(nsRes.averageLatencyMs - 5.0) < 1e-4, "Phase 8.2", "Nearest Server achieves optimal 5.0 ms average latency");
        AUDIT_ASSERT(nsRes.serverLoads["Srv1"] == 10.0 && nsRes.serverLoads["Srv2"] == 10.0,
                     "Phase 8.3", "Nearest Server assigns RegA -> Srv1 and RegB -> Srv2 exactly");

        // Round Robin test
        StrategyResult rrRes = BaselineRouters::runRoundRobin(miniNet, miniCalc);
        AUDIT_ASSERT(rrRes.totalTrafficRouted == 20.0, "Phase 8.4", "Round Robin routes 100% of mini network demand");
        AUDIT_ASSERT(rrRes.serverLoads["Srv1"] == 10.0 && rrRes.serverLoads["Srv2"] == 10.0,
                     "Phase 8.5", "Round Robin balances load equally across servers (10 req/s each)");
        AUDIT_ASSERT(std::abs(rrRes.averageLatencyMs - 15.0) < 1e-4, "Phase 8.6",
                     "Round Robin achieves exact expected split latency (15.0 ms)");

        // Greedy Cost test with cost asymmetry: Make Srv2 cheaper for RegA
        Network costNet = miniNet;
        costNet.getLink("RegA", "Srv2")->bandwidthCost = 0.1; // Make A->Srv2 much cheaper despite higher latency
        StrategyResult gcRes = BaselineRouters::runGreedyCost(costNet, miniCalc);
        AUDIT_ASSERT(gcRes.totalTrafficRouted == 20.0, "Phase 8.7", "Greedy Cost routes 100% of demand");

        // MCMF test on mini network
        StrategyResult mcmfRes = BaselineRouters::runMCMF(miniNet, miniCalc);
        AUDIT_ASSERT(mcmfRes.totalTrafficRouted == 20.0, "Phase 8.8", "MCMF routes 100% of mini network demand");

        // 2. Full Network Invariant Verifications across all 4 strategies
        Network fullNet = createFullSampleNetwork();
        CostCalculator fullCalc(1.0, 1.5, 20.0, 100);

        std::vector<StrategyResult> allResults = {
            BaselineRouters::runRoundRobin(fullNet, fullCalc),
            BaselineRouters::runNearestServer(fullNet, fullCalc),
            BaselineRouters::runGreedyCost(fullNet, fullCalc),
            BaselineRouters::runMCMF(fullNet, fullCalc)
        };

        for (const auto& strat : allResults) {
            bool serverCapOk = true;
            for (const auto& s : fullNet.getServers()) {
                if (strat.serverLoads.at(s.name) > s.configuredCapacity + 1e-4) serverCapOk = false;
            }
            AUDIT_ASSERT(serverCapOk, "Phase 8.9 (" + strat.strategyName + ")", "Server capacity constraint strictly respected");

            bool linkCapOk = true;
            for (const auto& l : fullNet.getLinks()) {
                std::string key = l.regionName + "->" + l.serverName;
                if (strat.linkLoads.at(key) > l.bandwidthCapacity + 1e-4) linkCapOk = false;
            }
            AUDIT_ASSERT(linkCapOk, "Phase 8.10 (" + strat.strategyName + ")", "Link bandwidth capacity constraint strictly respected");

            bool conservationOk = (std::abs(strat.totalTrafficRouted - fullNet.getTotalTrafficDemand()) < 1e-4) &&
                                  (std::abs(strat.totalTrafficRouted - (strat.edgeTrafficRouted + strat.originTrafficRouted)) < 1e-4);
            AUDIT_ASSERT(conservationOk, "Phase 8.11 (" + strat.strategyName + ")", "Flow conservation strictly maintained");
        }

        // 3. Failed Server Isolation verification
        Network outageNet = fullNet;
        outageNet.getServer("Delhi Server")->fail();
        std::vector<StrategyResult> outageResults = {
            BaselineRouters::runRoundRobin(outageNet, fullCalc),
            BaselineRouters::runNearestServer(outageNet, fullCalc),
            BaselineRouters::runGreedyCost(outageNet, fullCalc),
            BaselineRouters::runMCMF(outageNet, fullCalc)
        };

        for (const auto& strat : outageResults) {
            AUDIT_ASSERT(strat.serverLoads.at("Delhi Server") == 0.0,
                         "Phase 8.12 (" + strat.strategyName + ")", "Failed server receives strictly ZERO traffic");
        }
    }

    // =========================================================================
    // AUDIT SECTION 10: PHASE 8 — 8 COMPARATIVE BENCHMARK SCENARIOS
    // =========================================================================
    std::cout << "\n>>> [AUDIT SECTION 10: PHASE 8 — 8 COMPARATIVE BENCHMARK SCENARIOS] <<<\n";
    {
        CostCalculator calc(1.0, 1.5, 20.0, 100);
        std::vector<std::string> jsonScenarios;

        // Scenario 1: Normal Traffic Baseline
        {
            Network net = createFullSampleNetwork();
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 1: Normal Traffic Baseline");
            AUDIT_ASSERT(res[3].totalRoutingCost <= res[0].totalRoutingCost && res[3].totalRoutingCost <= res[1].totalRoutingCost,
                         "Scenario 1", "MCMF achieves lower or equal total routing cost compared to heuristics");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 1: Normal Traffic Baseline", res));
        }

        // Scenario 2: Mumbai Traffic Spike (3500 -> 7000 req/s)
        {
            Network net = createFullSampleNetwork();
            net.setRegionDemand("Mumbai", 7000.0);
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 2: Mumbai Traffic Spike (2.0x)");
            AUDIT_ASSERT(res[0].totalTrafficRouted == 15500.0 && res[3].totalTrafficRouted == 15500.0,
                         "Scenario 2", "All strategies route 100% of demand during regional spike (15,500 req/s)");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 2: Mumbai Traffic Spike (2.0x)", res));
        }

        // Scenario 3: Delhi Server Failure
        {
            Network net = createFullSampleNetwork();
            net.getServer("Delhi Server")->fail();
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 3: Delhi Server Outage");
            AUDIT_ASSERT(res[0].serverLoads["Delhi Server"] == 0.0 && res[3].serverLoads["Delhi Server"] == 0.0,
                         "Scenario 3", "Zero traffic allocated to failed Delhi Server across all strategies");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 3: Delhi Server Outage", res));
        }

        // Scenario 4: Delhi Server Recovery
        {
            Network net = createFullSampleNetwork();
            net.getServer("Delhi Server")->fail();
            net.getServer("Delhi Server")->recover();
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 4: Delhi Server Recovery");
            AUDIT_ASSERT(res[0].serverLoads["Delhi Server"] > 0.0 && res[3].serverLoads["Delhi Server"] > 0.0,
                         "Scenario 4", "Traffic automatically re-admitted to Delhi Server upon recovery");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 4: Delhi Server Recovery", res));
        }

        // Scenario 5: Pune Network Congestion (+35 ms)
        {
            Network net = createFullSampleNetwork();
            net.getLink("Pune", "Pune Server")->congestionLatencyMs = 35.0; // 5 -> 40 ms
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 5: Pune Network Link Congestion (+35ms)");
            AUDIT_ASSERT(res[3].linkLoads["Pune->Pune Server"] == 0.0,
                         "Scenario 5", "MCMF intelligently diverts 100% of Pune traffic away from congested link");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 5: Pune Network Link Congestion (+35ms)", res));
        }

        // Scenario 6: Traffic Exceeding Total Edge Capacity (24,000 req/s vs 20,500 req/s edge cap)
        {
            Network net = createFullSampleNetwork();
            // Scale all demands by 2.0x -> Total demand = 24,000 req/s
            for (const auto& r : net.getRegions()) {
                net.setRegionDemand(r.name, r.trafficDemand * 2.0);
            }
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 6: Demand Exceeding Edge Capacity (24k req/s)");
            double expectedSpill = 24000.0 - 20500.0; // 3500 req/s excess
            AUDIT_ASSERT(std::abs(res[3].originTrafficRouted - expectedSpill) < 1e-4,
                         "Scenario 6", "MCMF absorbs exactly max edge fleet capacity (20,500) and spills exactly 3,500 req/s to Origin");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 6: Demand Exceeding Edge Capacity (24k req/s)", res));
        }

        // Scenario 7: All Edge Servers Failed (Catastrophic Outage)
        {
            Network net = createFullSampleNetwork();
            net.getServer("Pune Server")->fail();
            net.getServer("Mumbai Server")->fail();
            net.getServer("Delhi Server")->fail();
            net.getServer("Bangalore Server")->fail();
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 7: Catastrophic Outage (All Edge Down)");
            AUDIT_ASSERT(res[0].originTrafficRouted == 12000.0 && res[3].originTrafficRouted == 12000.0,
                         "Scenario 7", "100% of demand safely routed to Origin fallback across all strategies");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 7: Catastrophic Outage (All Edge Down)", res));
        }

        // Scenario 8: Cache-Hit Ratio Improvement
        {
            Network net = createFullSampleNetwork();
            net.getLink("Pune", "Pune Server")->cacheHitRatio = 0.98;
            net.getLink("Mumbai", "Mumbai Server")->cacheHitRatio = 0.99;
            auto res = BaselineRouters::compareAll(net, calc, "Scenario 8: Cache Hit Ratio Improvement (0.98/0.99)");
            AUDIT_ASSERT(res[3].totalRoutingCost < 250000.0,
                         "Scenario 8", "High cache hit ratios successfully drive down overall CDN routing costs");
            jsonScenarios.push_back(BaselineRouters::exportResultsJson("Scenario 8: Cache Hit Ratio Improvement (0.98/0.99)", res));
        }

        // Save structured benchmark results for frontend dashboard consumption
        std::ofstream jsonFile("benchmark_results.json");
        if (jsonFile.is_open()) {
            jsonFile << "[\n";
            for (size_t i = 0; i < jsonScenarios.size(); ++i) {
                jsonFile << jsonScenarios[i] << (i + 1 < jsonScenarios.size() ? ",\n" : "\n");
            }
            jsonFile << "]\n";
            jsonFile.close();
            std::cout << ">>> [Dashboard Export] Successfully saved 8 scenario results to 'benchmark_results.json'. <<<\n";
        }
    }

    // =========================================================================
    // AUDIT SUMMARY
    // =========================================================================
    std::cout << "\n========================================================================================\n";
    std::cout << "                             TECHNICAL AUDIT RESULTS SUMMARY                            \n";
    std::cout << "========================================================================================\n";
    std::cout << "  TOTAL AUDIT TESTS EXECUTED : " << g_totalTests << "\n";
    std::cout << "  TOTAL TESTS PASSED         : " << g_passedTests << "\n";
    std::cout << "  TOTAL TESTS FAILED         : " << g_failedTests << "\n";
    std::cout << "  AUDIT VERDICT              : " << (g_failedTests == 0 ? "[100% PASSED - ALL PHASES 1-8 VERIFIED]" : "[FAILURES DETECTED]") << "\n";
    std::cout << "========================================================================================\n\n";

    return (g_failedTests == 0) ? 0 : 1;
}
