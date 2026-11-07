#ifndef FAILURE_HANDLER_H
#define FAILURE_HANDLER_H

#include "Network.h"
#include <string>
#include <vector>
#include <iostream>

/**
 * @brief Manages network failure events, server outages, recovery, and degradation.
 * 
 * Provides an isolated control layer to inject and monitor failures:
 * - Server crashes (hardware failure, maintenance)
 * - Server recoveries (node online, capacity restored)
 * - Peering link congestion (increased latency)
 * - Regional flash crowds / traffic spikes
 */
class FailureHandler {
private:
    Network& network;                       // Reference to the CDN network
    std::vector<std::string> incidentLog;   // Historical log of triggered failure events

public:
    explicit FailureHandler(Network& net);

    /**
     * @brief Simulates sudden edge server failure.
     * Sets server inactive, drops available capacity to 0, logs the event.
     */
    bool failServer(const std::string& serverName);

    /**
     * @brief Restores a failed edge server back online.
     * Sets server active, restores configured capacity, logs the event.
     */
    bool recoverServer(const std::string& serverName);

    /**
     * @brief Injects transient congestion onto a region-server interconnect link.
     */
    bool injectCongestion(const std::string& regionName, const std::string& serverName, double delayMs);

    /**
     * @brief Clears all injected congestion delays across all links.
     */
    void clearAllCongestion();

    /**
     * @brief Triggers an unexpected traffic spike in a specific region.
     */
    bool injectTrafficSpike(const std::string& regionName, double multiplier);

    /**
     * @brief Checks if a specific server is operational.
     */
    bool isServerHealthy(const std::string& serverName) const;

    /**
     * @brief Prints a formatted incident report of all past failure and recovery events.
     */
    void displayFailureReport() const;
};

#endif // FAILURE_HANDLER_H

