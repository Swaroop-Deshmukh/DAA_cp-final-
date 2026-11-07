#include "../include/Simulator.h"
#include <iostream>
#include <iomanip>
#include <cmath>

Simulator::Simulator(Network& net, unsigned int seed)
    : network(net), currentSeed(seed), rng(seed) {
    // Record initial configured demands as baseline
    for (const auto& region : network.getRegions()) {
        baseDemands[region.name] = region.trafficDemand;
    }
}

void Simulator::setRandomSeed(unsigned int seed) {
    currentSeed = seed;
    rng.seed(seed);
    std::cout << "[Simulator] Random seed set to: " << currentSeed << " (Deterministic replay enabled)\n";
}

void Simulator::generateNormalTraffic() {
    std::cout << "[Simulator] Generating Normal Baseline Traffic for all regions...\n";
    for (const auto& pair : baseDemands) {
        network.setRegionDemand(pair.first, pair.second);
    }
}

bool Simulator::createTrafficSpike(const std::string& regionName, double multiplier) {
    if (multiplier <= 0.0) {
        std::cerr << "[Simulator Error] Traffic spike multiplier must be positive.\n";
        return false;
    }

    Region* region = network.getRegion(regionName);
    if (region == nullptr) {
        std::cerr << "[Simulator Error] Region '" << regionName << "' not found for traffic spike.\n";
        return false;
    }

    double oldDemand = region->trafficDemand;
    double newDemand = oldDemand * multiplier;
    region->trafficDemand = newDemand;

    std::cout << "[Simulator Event] Traffic Spike in '" << regionName 
              << "': Demand increased from " << std::fixed << std::setprecision(0) 
              << oldDemand << " req/s to " << newDemand << " req/s (" 
              << std::fixed << std::setprecision(1) << multiplier << "x)\n";
    return true;
}

void Simulator::applyRandomTrafficVariation(double variationFraction) {
    if (variationFraction < 0.0 || variationFraction > 1.0) {
        std::cerr << "[Simulator Error] Variation fraction must be between 0.0 and 1.0.\n";
        return;
    }

    std::uniform_real_distribution<double> dist(-variationFraction, variationFraction);

    std::cout << "[Simulator] Applying +/-" << std::fixed << std::setprecision(0) 
              << (variationFraction * 100.0) << "% random traffic variation across all regions (Seed: " 
              << currentSeed << ")...\n";

    for (auto& region : network.getRegions()) {
        double base = baseDemands[region.name];
        double deltaPercentage = dist(rng);
        double perturbed = std::round(base * (1.0 + deltaPercentage));
        if (perturbed < 0.0) perturbed = 0.0;
        region.trafficDemand = perturbed;
    }
}

bool Simulator::failServer(const std::string& serverName) {
    Server* server = network.getServer(serverName);
    if (server == nullptr) {
        std::cerr << "[Simulator Error] Server '" << serverName << "' not found to fail.\n";
        return false;
    }

    if (!server->isActive) {
        std::cout << "[Simulator Notice] Server '" << serverName << "' is already inactive.\n";
        return true;
    }

    server->fail();
    std::cout << "[Simulator Event] [ALERT] Server OUTAGE: '" << serverName 
              << "' has FAILED! Available capacity dropped from " 
              << std::fixed << std::setprecision(0) << server->configuredCapacity 
              << " req/s to 0 req/s.\n";
    return true;
}

bool Simulator::recoverServer(const std::string& serverName) {
    Server* server = network.getServer(serverName);
    if (server == nullptr) {
        std::cerr << "[Simulator Error] Server '" << serverName << "' not found to recover.\n";
        return false;
    }

    if (server->isActive) {
        std::cout << "[Simulator Notice] Server '" << serverName << "' is already active.\n";
        return true;
    }

    server->recover();
    std::cout << "[Simulator Event] [RECOVERY] Server RESTORED: '" << serverName 
              << "' is back ONLINE. Available capacity restored to " 
              << std::fixed << std::setprecision(0) << server->availableCapacity << " req/s.\n";
    return true;
}

bool Simulator::addNetworkCongestion(const std::string& regionName, const std::string& serverName, 
                                     double additionalLatencyMs) {
    NetworkLink* link = network.getLink(regionName, serverName);
    if (link == nullptr) {
        std::cerr << "[Simulator Error] Link between '" << regionName 
                  << "' and '" << serverName << "' not found.\n";
        return false;
    }

    link->congestionLatencyMs += additionalLatencyMs;
    std::cout << "[Simulator Event] Network Congestion on link [" << regionName << " -> " << serverName 
              << "]: Added +" << std::fixed << std::setprecision(1) << additionalLatencyMs 
              << " ms (Effective latency is now " << link->getEffectiveLatency() << " ms)\n";
    return true;
}

void Simulator::resetNetworkCongestion() {
    for (auto& link : network.getLinks()) {
        link.congestionLatencyMs = 0.0;
    }
    std::cout << "[Simulator] All network link congestion delays reset to 0 ms.\n";
}

void Simulator::displaySimulationStatus() const {
    std::cout << "\n==================================================================================\n";
    std::cout << "                        SIMULATION REAL-TIME STATUS                               \n";
    std::cout << "==================================================================================\n";

    std::cout << "--- [Regional Traffic Demand] ---\n";
    std::cout << "+-----------------+--------------------+--------------------+----------------+\n";
    std::cout << "| " << std::left << std::setw(15) << "Region"
              << " | " << std::setw(18) << "Base Demand (req/s)"
              << " | " << std::setw(18) << "Current Demand"
              << " | " << std::setw(14) << "Variation (%)" << " |\n";
    std::cout << "+-----------------+--------------------+--------------------+----------------+\n";

    for (const auto& region : network.getRegions()) {
        double base = baseDemands.at(region.name);
        double current = region.trafficDemand;
        double diffPct = (base > 0.0) ? ((current - base) / base) * 100.0 : 0.0;
        
        std::cout << "| " << std::left << std::setw(15) << region.name
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << base
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << current
                  << " | " << std::right << std::setw(13) << std::fixed << std::setprecision(1) 
                           << (diffPct >= 0 ? "+" : "") << diffPct << "% |\n";
    }
    std::cout << "+-----------------+--------------------+--------------------+----------------+\n";
    std::cout << "  Total Current Demand: " << std::fixed << std::setprecision(0) 
              << network.getTotalTrafficDemand() << " req/s\n";

    std::cout << "\n--- [Edge Server Health & Availability] ---\n";
    network.displayServers();

    std::cout << "\n--- [Congested Links Summary] ---\n";
    bool anyCongestion = false;
    for (const auto& link : network.getLinks()) {
        if (link.congestionLatencyMs > 0.0) {
            anyCongestion = true;
            std::cout << "  - [" << link.regionName << " -> " << link.serverName << "] Base: "
                      << link.baseLatencyMs << " ms | Added Delay: +" << link.congestionLatencyMs 
                      << " ms | Effective: " << link.getEffectiveLatency() << " ms\n";
        }
    }
    if (!anyCongestion) {
        std::cout << "  (All links operating at optimal base latency, zero congestion detected)\n";
    }
    std::cout << "==================================================================================\n\n";
}

void Simulator::runScenarioNormalTraffic() {
    std::cout << "\n>>>>>>>>>>>>>>>>>>>> [SCENARIO 1: NORMAL BASELINE TRAFFIC] <<<<<<<<<<<<<<<<<<<<\n";
    generateNormalTraffic();
    resetNetworkCongestion();
    // Ensure all servers are recovered
    for (auto& server : network.getServers()) {
        if (!server.isActive) {
            recoverServer(server.name);
        }
    }
    displaySimulationStatus();
}

void Simulator::runScenarioMumbaiTrafficSpike() {
    std::cout << "\n>>>>>>>>>>>>>>>>>>>> [SCENARIO 2: MUMBAI TRAFFIC SPIKE] <<<<<<<<<<<<<<<<<<<<\n";
    // Reset to normal first
    generateNormalTraffic();
    resetNetworkCongestion();

    // Trigger 2.0x traffic spike in Mumbai
    createTrafficSpike("Mumbai", 2.0);

    // Congestion develops on Mumbai links due to heavy network transit
    addNetworkCongestion("Mumbai", "Mumbai Server", 25.0);
    addNetworkCongestion("Mumbai", "Pune Server", 18.0);

    displaySimulationStatus();
}

void Simulator::runScenarioDelhiServerFailure() {
    std::cout << "\n>>>>>>>>>>>>>>>>>>>> [SCENARIO 3: DELHI SERVER OUTAGE & FAILOVER] <<<<<<<<<<<<<<<<<<<<\n";
    // Keep baseline traffic
    generateNormalTraffic();
    resetNetworkCongestion();

    // Delhi server suffers hardware failure
    failServer("Delhi Server");

    displaySimulationStatus();
}

