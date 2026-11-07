#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "Network.h"
#include <string>
#include <map>
#include <random>

/**
 * @brief Network Traffic Simulator for the Adaptive CDN Traffic Router.
 * 
 * Simulates dynamic, realistic network conditions:
 * - Normal baseline user traffic
 * - Regional flash crowds and traffic spikes
 * - Controlled pseudo-random traffic fluctuation with fixed seed reproducibility
 * - Server outages and automated recovery
 * - Transient link congestion and latency degradation
 */
class Simulator {
private:
    Network& network;                            // Reference to the underlying CDN network
    std::map<std::string, double> baseDemands;  // Baseline traffic demand per region (req/s)
    unsigned int currentSeed;                    // Seed used for reproducible pseudorandom generation
    std::mt19937 rng;                            // Mersenne Twister RNG engine

public:
    /**
     * @brief Constructs a simulator instance bound to a network.
     * @param net Reference to the CDN network to simulate.
     * @param seed Initial random seed for deterministic reproduction.
     */
    explicit Simulator(Network& net, unsigned int seed = 42);

    /**
     * @brief Sets or resets the random seed for deterministic simulation.
     * @param seed Unsigned integer seed value.
     */
    void setRandomSeed(unsigned int seed);

    /**
     * @brief Resets all regional traffic demands back to their initial baseline levels.
     */
    void generateNormalTraffic();

    /**
     * @brief Induces a traffic spike in a specific region by multiplying its demand.
     * @param regionName Name of the region experiencing the spike.
     * @param multiplier Demand multiplier (e.g., 2.0 = 200% traffic).
     * @return true if successful, false if region not found or invalid multiplier.
     */
    bool createTrafficSpike(const std::string& regionName, double multiplier);

    /**
     * @brief Applies pseudo-random bounded percentage fluctuation to all regions.
     * @param variationFraction Maximum percentage variance (e.g., 0.15 = +/- 15%).
     */
    void applyRandomTrafficVariation(double variationFraction);

    /**
     * @brief Simulates an edge server failure event.
     * Marks server inactive and drops available capacity to zero while preserving configured capacity.
     * @param serverName Name of the server that failed.
     * @return true if server found and failed, false otherwise.
     */
    bool failServer(const std::string& serverName);

    /**
     * @brief Simulates an edge server recovery event.
     * Marks server active and restores its available capacity back to full configured capacity.
     * @param serverName Name of the recovering server.
     * @return true if server found and recovered, false otherwise.
     */
    bool recoverServer(const std::string& serverName);

    /**
     * @brief Adds transient congestion latency to a specific region-server connection.
     * @param regionName Originating region name.
     * @param serverName Destination server name.
     * @param additionalLatencyMs Additional latency in milliseconds.
     * @return true if link exists and updated, false otherwise.
     */
    bool addNetworkCongestion(const std::string& regionName, const std::string& serverName, 
                             double additionalLatencyMs);

    /**
     * @brief Clears all additional congestion latencies across all network links.
     */
    void resetNetworkCongestion();

    /**
     * @brief Prints a formatted diagnostic summary of current simulation state.
     */
    void displaySimulationStatus() const;

    // Pre-built repeatable scenario drivers
    void runScenarioNormalTraffic();
    void runScenarioMumbaiTrafficSpike();
    void runScenarioDelhiServerFailure();
};

#endif // SIMULATOR_H

