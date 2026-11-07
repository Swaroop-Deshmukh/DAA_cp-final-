#include "../include/Network.h"
#include <iostream>
#include <iomanip>

// ==========================================
// Region Struct Implementation
// ==========================================
Region::Region(const std::string& regionName, double demand)
    : name(regionName), trafficDemand(demand) {}

// ==========================================
// Server Struct Implementation
// ==========================================
Server::Server(const std::string& serverName, double capacity, bool active)
    : name(serverName),
      maxCapacity(capacity),
      configuredCapacity(capacity),
      availableCapacity(active ? capacity : 0.0),
      isActive(active) {}

void Server::fail() {
    isActive = false;
    availableCapacity = 0.0;
}

void Server::recover() {
    isActive = true;
    availableCapacity = configuredCapacity;
}

// ==========================================
// NetworkLink Struct Implementation
// ==========================================
NetworkLink::NetworkLink(const std::string& reg, const std::string& srv, double baseLatency, 
                         double cost, double bwCapacity, double hitRatio)
    : regionName(reg),
      serverName(srv),
      baseLatencyMs(baseLatency),
      congestionLatencyMs(0.0),
      bandwidthCost(cost),
      bandwidthCapacity(bwCapacity),
      cacheHitRatio(hitRatio) {}

double NetworkLink::getEffectiveLatency() const {
    return baseLatencyMs + congestionLatencyMs;
}

// ==========================================
// Network Class Implementation
// ==========================================

bool Network::addRegion(const std::string& regionName, double trafficDemand) {
    if (regionName.empty()) {
        std::cerr << "[Error] Cannot add region: Region name cannot be empty.\n";
        return false;
    }

    if (trafficDemand < 0.0) {
        std::cerr << "[Error] Cannot add region '" << regionName 
                  << "': Traffic demand cannot be negative (" << trafficDemand << ").\n";
        return false;
    }

    // Check for duplicate region name
    if (getRegion(regionName) != nullptr) {
        std::cerr << "[Warning] Region '" << regionName << "' already exists. Updating demand.\n";
        setRegionDemand(regionName, trafficDemand);
        return true;
    }

    regions.emplace_back(regionName, trafficDemand);
    return true;
}

bool Network::addServer(const std::string& serverName, double maxCapacity, bool isActive) {
    if (serverName.empty()) {
        std::cerr << "[Error] Cannot add server: Server name cannot be empty.\n";
        return false;
    }

    if (maxCapacity <= 0.0) {
        std::cerr << "[Error] Cannot add server '" << serverName 
                  << "': Maximum capacity must be greater than zero (" << maxCapacity << ").\n";
        return false;
    }

    if (getServer(serverName) != nullptr) {
        std::cerr << "[Warning] Server '" << serverName << "' already exists.\n";
        return false;
    }

    servers.emplace_back(serverName, maxCapacity, isActive);
    return true;
}

bool Network::addLink(const std::string& regionName, const std::string& serverName, 
                      double baseLatencyMs, double bandwidthCost,
                      double bandwidthCapacity, double cacheHitRatio) {
    if (getRegion(regionName) == nullptr) {
        std::cerr << "[Error] Cannot add link: Region '" << regionName << "' not found.\n";
        return false;
    }
    if (getServer(serverName) == nullptr) {
        std::cerr << "[Error] Cannot add link: Server '" << serverName << "' not found.\n";
        return false;
    }
    if (baseLatencyMs < 0.0 || bandwidthCost < 0.0) {
        std::cerr << "[Error] Latency and bandwidth cost must be non-negative.\n";
        return false;
    }
    if (bandwidthCapacity <= 0.0) {
        std::cerr << "[Error] Link bandwidth capacity must be strictly positive (" << bandwidthCapacity << ").\n";
        return false;
    }
    if (cacheHitRatio < 0.0 || cacheHitRatio > 1.0) {
        std::cerr << "[Error] Cache hit ratio must be between 0.0 and 1.0 (" << cacheHitRatio << ").\n";
        return false;
    }

    // Overwrite existing link if present
    NetworkLink* existing = getLink(regionName, serverName);
    if (existing != nullptr) {
        existing->baseLatencyMs = baseLatencyMs;
        existing->bandwidthCost = bandwidthCost;
        existing->bandwidthCapacity = bandwidthCapacity;
        existing->cacheHitRatio = cacheHitRatio;
        return true;
    }

    links.emplace_back(regionName, serverName, baseLatencyMs, bandwidthCost, bandwidthCapacity, cacheHitRatio);
    return true;
}

bool Network::setRegionDemand(const std::string& regionName, double newDemand) {
    if (newDemand < 0.0) {
        std::cerr << "[Error] Demand cannot be negative (" << newDemand << ").\n";
        return false;
    }
    Region* r = getRegion(regionName);
    if (r == nullptr) {
        std::cerr << "[Error] Region '" << regionName << "' not found.\n";
        return false;
    }
    r->trafficDemand = newDemand;
    return true;
}

Region* Network::getRegion(const std::string& regionName) {
    for (auto& r : regions) {
        if (r.name == regionName) return &r;
    }
    return nullptr;
}

const Region* Network::getRegion(const std::string& regionName) const {
    for (const auto& r : regions) {
        if (r.name == regionName) return &r;
    }
    return nullptr;
}

Server* Network::getServer(const std::string& serverName) {
    for (auto& s : servers) {
        if (s.name == serverName) return &s;
    }
    return nullptr;
}

const Server* Network::getServer(const std::string& serverName) const {
    for (const auto& s : servers) {
        if (s.name == serverName) return &s;
    }
    return nullptr;
}

NetworkLink* Network::getLink(const std::string& regionName, const std::string& serverName) {
    for (auto& link : links) {
        if (link.regionName == regionName && link.serverName == serverName) {
            return &link;
        }
    }
    return nullptr;
}

const NetworkLink* Network::getLink(const std::string& regionName, const std::string& serverName) const {
    for (const auto& link : links) {
        if (link.regionName == regionName && link.serverName == serverName) {
            return &link;
        }
    }
    return nullptr;
}

void Network::displayRegions() const {
    std::cout << "\n+----------------------+--------------------------+\n";
    std::cout << "| " << std::left << std::setw(20) << "Region Name" 
              << " | " << std::setw(24) << "Traffic Demand (req/s)" << " |\n";
    std::cout << "+----------------------+--------------------------+\n";

    if (regions.empty()) {
        std::cout << "| " << std::setw(47) << "No regions registered." << " |\n";
    } else {
        for (const auto& region : regions) {
            std::cout << "| " << std::left << std::setw(20) << region.name 
                      << " | " << std::right << std::setw(24) << std::fixed << std::setprecision(0) << region.trafficDemand << " |\n";
        }
    }
    std::cout << "+----------------------+--------------------------+\n";
    std::cout << "  Total Demand: " << std::fixed << std::setprecision(0) 
              << getTotalTrafficDemand() << " req/s\n";
}

void Network::displayServers() const {
    std::cout << "\n+----------------------+--------------------+--------------------+------------+\n";
    std::cout << "| " << std::left << std::setw(20) << "Server Name" 
              << " | " << std::setw(18) << "Configured (req/s)" 
              << " | " << std::setw(18) << "Available (req/s)"
              << " | " << std::setw(10) << "Status" << " |\n";
    std::cout << "+----------------------+--------------------+--------------------+------------+\n";

    if (servers.empty()) {
        std::cout << "| " << std::setw(76) << "No servers registered." << " |\n";
    } else {
        for (const auto& server : servers) {
            std::string statusStr = server.isActive ? "ACTIVE" : "INACTIVE";
            std::cout << "| " << std::left << std::setw(20) << server.name 
                      << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << server.configuredCapacity
                      << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << server.availableCapacity 
                      << " | " << std::left << std::setw(10) << statusStr << " |\n";
        }
    }
    std::cout << "+----------------------+--------------------+--------------------+------------+\n";
    std::cout << "  Configured Total: " << std::fixed << std::setprecision(0) << getTotalConfiguredServerCapacity() 
              << " req/s | Active Available: " << getTotalActiveServerCapacity() << " req/s\n";
}

void Network::displayLinks() const {
    std::cout << "\n+-----------------+---------------------+-------------+----------------+-----------------+\n";
    std::cout << "| " << std::left << std::setw(15) << "Region" 
              << " | " << std::setw(19) << "Target Server" 
              << " | " << std::setw(11) << "Base Latency" 
              << " | " << std::setw(14) << "Congestion" 
              << " | " << std::setw(15) << "Effective Latency" << " |\n";
    std::cout << "+-----------------+---------------------+-------------+----------------+-----------------+\n";

    if (links.empty()) {
        std::cout << "| " << std::setw(85) << "No links defined." << " |\n";
    } else {
        for (const auto& link : links) {
            std::cout << "| " << std::left << std::setw(15) << link.regionName 
                      << " | " << std::setw(19) << link.serverName 
                      << " | " << std::right << std::setw(8) << std::fixed << std::setprecision(1) << link.baseLatencyMs << " ms"
                      << " | " << std::right << std::setw(9) << (link.congestionLatencyMs > 0 ? "+" : " ") 
                               << std::fixed << std::setprecision(1) << link.congestionLatencyMs << " ms"
                      << " | " << std::right << std::setw(10) << std::fixed << std::setprecision(1) << link.getEffectiveLatency() << " ms" << "   |\n";
        }
    }
    std::cout << "+-----------------+---------------------+-------------+----------------+-----------------+\n";
}

void Network::displayNetwork() const {
    std::cout << "==================================================================================\n";
    std::cout << "                   ADAPTIVE CDN TRAFFIC ROUTER - TOPOLOGY                         \n";
    std::cout << "==================================================================================\n";
    
    std::cout << "\n--- [Registered Client Regions] ---";
    displayRegions();

    std::cout << "\n--- [Registered CDN Edge Servers] ---";
    displayServers();

    std::cout << "\n--- [Network Interconnect Links] ---";
    displayLinks();

    std::cout << "\n--- [Network Capacity Summary] ---\n";
    double totalDemand = getTotalTrafficDemand();
    double totalActiveCapacity = getTotalActiveServerCapacity();
    double totalConfigured = getTotalConfiguredServerCapacity();

    std::cout << "  Total User Demand            : " << totalDemand << " req/s\n";
    std::cout << "  Configured Total Capacity    : " << totalConfigured << " req/s\n";
    std::cout << "  Currently Available Capacity : " << totalActiveCapacity << " req/s\n";
    
    if (totalActiveCapacity >= totalDemand) {
        std::cout << "  Available Capacity Margin    : +" << (totalActiveCapacity - totalDemand) 
                  << " req/s (Healthy: capacity satisfies total demand)\n";
    } else {
        std::cout << "  Available Capacity Deficit   : -" << (totalDemand - totalActiveCapacity) 
                  << " req/s [CRITICAL: Capacity deficit! Traffic shedding or degradation required]\n";
    }
    std::cout << "==================================================================================\n\n";
}

const std::vector<Region>& Network::getRegions() const { return regions; }
std::vector<Region>& Network::getRegions() { return regions; }

const std::vector<Server>& Network::getServers() const { return servers; }
std::vector<Server>& Network::getServers() { return servers; }

const std::vector<NetworkLink>& Network::getLinks() const { return links; }
std::vector<NetworkLink>& Network::getLinks() { return links; }

double Network::getTotalTrafficDemand() const {
    double total = 0.0;
    for (const auto& region : regions) {
        total += region.trafficDemand;
    }
    return total;
}

double Network::getTotalActiveServerCapacity() const {
    double total = 0.0;
    for (const auto& server : servers) {
        if (server.isActive) {
            total += server.availableCapacity;
        }
    }
    return total;
}

double Network::getTotalConfiguredServerCapacity() const {
    double total = 0.0;
    for (const auto& server : servers) {
        total += server.configuredCapacity;
    }
    return total;
}
