#include "../include/FailureHandler.h"
#include <iostream>
#include <iomanip>

FailureHandler::FailureHandler(Network& net) : network(net) {}

bool FailureHandler::failServer(const std::string& serverName) {
    Server* server = network.getServer(serverName);
    if (!server) {
        std::cerr << "[FailureHandler Error] Server '" << serverName << "' not found.\n";
        return false;
    }

    if (!server->isActive) {
        std::cout << "[FailureHandler Notice] Server '" << serverName << "' is already offline.\n";
        return true;
    }

    server->fail();
    std::string msg = "[OUTAGE EVENT] Server '" + serverName + "' FAILED. Available capacity dropped to 0 req/s.";
    incidentLog.push_back(msg);
    std::cout << "\n>>> " << msg << " <<<\n";
    return true;
}

bool FailureHandler::recoverServer(const std::string& serverName) {
    Server* server = network.getServer(serverName);
    if (!server) {
        std::cerr << "[FailureHandler Error] Server '" << serverName << "' not found.\n";
        return false;
    }

    if (server->isActive) {
        std::cout << "[FailureHandler Notice] Server '" << serverName << "' is already online.\n";
        return true;
    }

    server->recover();
    std::string msg = "[RECOVERY EVENT] Server '" + serverName + "' RESTORED. Available capacity restored to " 
                    + std::to_string(static_cast<int>(server->availableCapacity)) + " req/s.";
    incidentLog.push_back(msg);
    std::cout << "\n>>> " << msg << " <<<\n";
    return true;
}

bool FailureHandler::injectCongestion(const std::string& regionName, const std::string& serverName, double delayMs) {
    NetworkLink* link = network.getLink(regionName, serverName);
    if (!link) {
        std::cerr << "[FailureHandler Error] Link [" << regionName << " -> " << serverName << "] not found.\n";
        return false;
    }

    link->congestionLatencyMs += delayMs;
    std::string msg = "[CONGESTION EVENT] Link [" + regionName + " -> " + serverName + "] delayed by +" 
                    + std::to_string(static_cast<int>(delayMs)) + " ms (Effective: " 
                    + std::to_string(static_cast<int>(link->getEffectiveLatency())) + " ms).";
    incidentLog.push_back(msg);
    std::cout << "\n>>> " << msg << " <<<\n";
    return true;
}

void FailureHandler::clearAllCongestion() {
    for (auto& link : network.getLinks()) {
        link.congestionLatencyMs = 0.0;
    }
    std::string msg = "[CONGESTION CLEARED] All transit link delays reset to 0 ms.";
    incidentLog.push_back(msg);
    std::cout << "\n>>> " << msg << " <<<\n";
}

bool FailureHandler::injectTrafficSpike(const std::string& regionName, double multiplier) {
    Region* region = network.getRegion(regionName);
    if (!region) {
        std::cerr << "[FailureHandler Error] Region '" << regionName << "' not found.\n";
        return false;
    }

    double oldDemand = region->trafficDemand;
    region->trafficDemand = oldDemand * multiplier;
    std::string msg = "[TRAFFIC SPIKE EVENT] Region '" + regionName + "' demand spiked from " 
                    + std::to_string(static_cast<int>(oldDemand)) + " to " 
                    + std::to_string(static_cast<int>(region->trafficDemand)) + " req/s ("
                    + std::to_string(multiplier).substr(0, 3) + "x).";
    incidentLog.push_back(msg);
    std::cout << "\n>>> " << msg << " <<<\n";
    return true;
}

bool FailureHandler::isServerHealthy(const std::string& serverName) const {
    const Server* server = network.getServer(serverName);
    return server ? server->isActive : false;
}

void FailureHandler::displayFailureReport() const {
    std::cout << "\n+-------------------------------------------------------------------------+\n";
    std::cout << "|                      FAILURE HANDLER INCIDENT LOG                       |\n";
    std::cout << "+-------------------------------------------------------------------------+\n";
    if (incidentLog.empty()) {
        std::cout << "| (No incidents recorded. Network operating under nominal conditions.)     |\n";
    } else {
        for (size_t i = 0; i < incidentLog.size(); ++i) {
            std::cout << "| " << std::left << std::setw(3) << (i + 1) << ". " 
                      << std::setw(67) << incidentLog[i] << " |\n";
        }
    }
    std::cout << "+-------------------------------------------------------------------------+\n\n";
}

