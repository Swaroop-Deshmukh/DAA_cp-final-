#ifndef COST_CALCULATOR_H
#define COST_CALCULATOR_H

#include <string>
#include <iostream>

/**
 * @brief Parameters defining the cost profile of routing traffic along a region-server connection.
 * 
 * Captures the multi-dimensional factors that determine routing desirability:
 * - Network latency (user experience)
 * - Bandwidth transit charges (economic cost)
 * - Cache hit efficiency and miss penalty (origin load and content delivery overhead)
 */
struct RouteConnectionProfile {
    std::string regionName;     // Source client region
    std::string serverName;     // Destination edge server
    double latencyMs;          // Network latency L (milliseconds)
    double bandwidthCost;      // Bandwidth transit cost B (e.g., $/Gbps)
    double cacheHitRatio;      // Cache hit ratio H in range [0.0, 1.0]
    double cacheMissPenalty;   // Penalty M incurred when content must be fetched from origin

    RouteConnectionProfile(const std::string& reg = "",
                           const std::string& srv = "",
                           double latency = 0.0,
                           double bwCost = 0.0,
                           double hitRatio = 0.8,
                           double missPenalty = 15.0);
};

/**
 * @brief Detailed breakdown of the calculated routing cost components.
 * 
 * Separates the contribution of latency, bandwidth, and cache misses so network
 * operators can inspect why a particular route was chosen.
 */
struct CostBreakdown {
    double latencyComponent;       // alpha * L
    double bandwidthComponent;     // beta * B
    double cacheMissComponent;     // (1 - H) * M
    double totalContinuousCost;    // C = alpha*L + beta*B + (1-H)*M
    long long scaledIntegerCost;   // Discretized non-negative integer cost for graph flow algorithms
};

/**
 * @brief Calculates the composite routing cost for traffic distribution.
 * 
 * Implements the formula:
 *   C = alpha * L + beta * B + (1 - H) * M
 * 
 * where:
 *   L = Network latency in milliseconds
 *   B = Bandwidth transit cost
 *   H = Cache hit ratio (0.0 to 1.0)
 *   M = Cache miss penalty
 *   alpha, beta = Configurable weighting factors
 * 
 * Also provides deterministic integer scaling suitable for Min-Cost Max-Flow algorithms.
 */
class CostCalculator {
private:
    double alpha;                  // Latency weighting factor (default: 1.0)
    double beta;                   // Bandwidth cost weighting factor (default: 1.5)
    double defaultMissPenalty;     // Default cache miss penalty (default: 15.0)
    long long costScalingFactor;   // Discretization multiplier for flow algorithms (default: 100)

public:
    /**
     * @brief Constructs a CostCalculator with configurable weights.
     */
    CostCalculator(double alphaWeight = 1.0, 
                   double betaWeight = 1.5, 
                   double missPenalty = 15.0, 
                   long long scalingFactor = 100);

    // Weight configuration mutators and accessors
    bool setAlpha(double a);
    bool setBeta(double b);
    bool setDefaultMissPenalty(double penalty);
    bool setCostScalingFactor(long long factor);

    double getAlpha() const;
    double getBeta() const;
    double getDefaultMissPenalty() const;
    long long getCostScalingFactor() const;

    /**
     * @brief Validates all fields of a connection profile.
     * @param profile Profile to validate.
     * @param outErrorMessage Populated with explanation if validation fails.
     * @return true if valid, false otherwise.
     */
    static bool validateProfile(const RouteConnectionProfile& profile, std::string& outErrorMessage);

    /**
     * @brief Computes continuous cost components and scaled integer cost.
     * @param profile Connection parameters.
     * @param outBreakdown Destination struct for cost components.
     * @return true if calculated successfully, false if profile is invalid.
     */
    bool calculateCost(const RouteConnectionProfile& profile, CostBreakdown& outBreakdown) const;

    /**
     * @brief Convenience helper returning only the scaled non-negative integer cost for flow network edges.
     * @param profile Connection parameters.
     * @return Scaled integer cost, or -1 on validation failure.
     */
    long long getScaledEdgeCost(const RouteConnectionProfile& profile) const;

    /**
     * @brief Formats and prints the cost components for visual inspection.
     */
    void printCostBreakdown(const RouteConnectionProfile& profile, const CostBreakdown& breakdown) const;

    /**
     * @brief Compares two connection profiles and prints their side-by-side cost trade-offs.
     */
    void compareConnections(const RouteConnectionProfile& connA, const RouteConnectionProfile& connB) const;
};

#endif // COST_CALCULATOR_H

