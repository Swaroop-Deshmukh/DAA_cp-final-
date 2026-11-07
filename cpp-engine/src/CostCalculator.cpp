#include "../include/CostCalculator.h"
#include <iostream>
#include <iomanip>
#include <cmath>

// ==========================================
// RouteConnectionProfile Implementation
// ==========================================
RouteConnectionProfile::RouteConnectionProfile(const std::string& reg,
                                               const std::string& srv,
                                               double latency,
                                               double bwCost,
                                               double hitRatio,
                                               double missPenalty)
    : regionName(reg),
      serverName(srv),
      latencyMs(latency),
      bandwidthCost(bwCost),
      cacheHitRatio(hitRatio),
      cacheMissPenalty(missPenalty) {}

// ==========================================
// CostCalculator Implementation
// ==========================================
CostCalculator::CostCalculator(double alphaWeight, 
                               double betaWeight, 
                               double missPenalty, 
                               long long scalingFactor)
    : alpha(alphaWeight),
      beta(betaWeight),
      defaultMissPenalty(missPenalty),
      costScalingFactor(scalingFactor) {
    if (alpha < 0.0) alpha = 1.0;
    if (beta < 0.0) beta = 1.5;
    if (defaultMissPenalty < 0.0) defaultMissPenalty = 15.0;
    if (costScalingFactor < 1) costScalingFactor = 100;
}

bool CostCalculator::setAlpha(double a) {
    if (a < 0.0) {
        std::cerr << "[CostCalculator Error] Alpha weight cannot be negative (" << a << ").\n";
        return false;
    }
    alpha = a;
    return true;
}

bool CostCalculator::setBeta(double b) {
    if (b < 0.0) {
        std::cerr << "[CostCalculator Error] Beta weight cannot be negative (" << b << ").\n";
        return false;
    }
    beta = b;
    return true;
}

bool CostCalculator::setDefaultMissPenalty(double penalty) {
    if (penalty < 0.0) {
        std::cerr << "[CostCalculator Error] Miss penalty cannot be negative (" << penalty << ").\n";
        return false;
    }
    defaultMissPenalty = penalty;
    return true;
}

bool CostCalculator::setCostScalingFactor(long long factor) {
    if (factor < 1) {
        std::cerr << "[CostCalculator Error] Cost scaling factor must be at least 1.\n";
        return false;
    }
    costScalingFactor = factor;
    return true;
}

double CostCalculator::getAlpha() const { return alpha; }
double CostCalculator::getBeta() const { return beta; }
double CostCalculator::getDefaultMissPenalty() const { return defaultMissPenalty; }
long long CostCalculator::getCostScalingFactor() const { return costScalingFactor; }

bool CostCalculator::validateProfile(const RouteConnectionProfile& profile, std::string& outErrorMessage) {
    if (profile.latencyMs < 0.0) {
        outErrorMessage = "Network latency cannot be negative (" + std::to_string(profile.latencyMs) + " ms).";
        return false;
    }

    if (profile.bandwidthCost < 0.0) {
        outErrorMessage = "Bandwidth cost cannot be negative (" + std::to_string(profile.bandwidthCost) + ").";
        return false;
    }

    if (profile.cacheHitRatio < 0.0 || profile.cacheHitRatio > 1.0) {
        outErrorMessage = "Cache hit ratio H must be strictly between 0.0 and 1.0 (Received: " 
                          + std::to_string(profile.cacheHitRatio) + ").";
        return false;
    }

    if (profile.cacheMissPenalty < 0.0) {
        outErrorMessage = "Cache miss penalty cannot be negative (" + std::to_string(profile.cacheMissPenalty) + ").";
        return false;
    }

    return true;
}

bool CostCalculator::calculateCost(const RouteConnectionProfile& profile, CostBreakdown& outBreakdown) const {
    std::string errorMsg;
    if (!validateProfile(profile, errorMsg)) {
        std::cerr << "[CostCalculator Error] Invalid connection profile [" 
                  << profile.regionName << " -> " << profile.serverName << "]: " 
                  << errorMsg << "\n";
        return false;
    }

    // Individual cost component evaluation: C = alpha * L + beta * B + (1 - H) * M
    outBreakdown.latencyComponent = alpha * profile.latencyMs;
    outBreakdown.bandwidthComponent = beta * profile.bandwidthCost;
    outBreakdown.cacheMissComponent = (1.0 - profile.cacheHitRatio) * profile.cacheMissPenalty;

    outBreakdown.totalContinuousCost = outBreakdown.latencyComponent 
                                     + outBreakdown.bandwidthComponent 
                                     + outBreakdown.cacheMissComponent;

    // Integer scaling for graph flow algorithms:
    // Ensures strictly non-negative integers and preserves precision up to two decimal places (when factor=100)
    long long scaled = static_cast<long long>(std::round(outBreakdown.totalContinuousCost * static_cast<double>(costScalingFactor)));
    outBreakdown.scaledIntegerCost = (scaled >= 0) ? scaled : 0;

    return true;
}

long long CostCalculator::getScaledEdgeCost(const RouteConnectionProfile& profile) const {
    CostBreakdown breakdown;
    if (calculateCost(profile, breakdown)) {
        return breakdown.scaledIntegerCost;
    }
    return -1;
}

void CostCalculator::printCostBreakdown(const RouteConnectionProfile& profile, const CostBreakdown& breakdown) const {
    std::cout << "+-------------------------------------------------------------------------+\n";
    std::cout << "| ROUTE COST BREAKDOWN: " << std::left << std::setw(15) << profile.regionName 
              << " -> " << std::setw(30) << profile.serverName << " |\n";
    std::cout << "+-------------------------------------------------------------------------+\n";
    std::cout << "|  - Latency (L)            : " << std::right << std::setw(8) << std::fixed << std::setprecision(1) 
              << profile.latencyMs << " ms  (weight alpha = " << alpha << ") => " 
              << std::setw(8) << std::setprecision(2) << breakdown.latencyComponent << " cost units |\n";
    std::cout << "|  - Bandwidth Cost (B)     : " << std::right << std::setw(8) << std::fixed << std::setprecision(2) 
              << profile.bandwidthCost << " $/unit(weight beta  = " << beta << ") => " 
              << std::setw(8) << std::setprecision(2) << breakdown.bandwidthComponent << " cost units |\n";
    std::cout << "|  - Cache Hit Ratio (H)    : " << std::right << std::setw(7) << std::fixed << std::setprecision(1) 
              << (profile.cacheHitRatio * 100.0) << "% (Miss penalty M = " << std::setprecision(1) 
              << profile.cacheMissPenalty << ") => " << std::setw(8) << std::setprecision(2) 
              << breakdown.cacheMissComponent << " cost units |\n";
    std::cout << "+-------------------------------------------------------------------------+\n";
    std::cout << "|  TOTAL CONTINUOUS COST (C) : " << std::right << std::setw(8) << std::fixed << std::setprecision(2) 
              << breakdown.totalContinuousCost << " cost units                       |\n";
    std::cout << "|  SCALED INTEGER COST (MCMF): " << std::right << std::setw(8) 
              << breakdown.scaledIntegerCost << " (scaling factor = " << costScalingFactor << "x)            |\n";
    std::cout << "+-------------------------------------------------------------------------+\n";
}

void CostCalculator::compareConnections(const RouteConnectionProfile& connA, const RouteConnectionProfile& connB) const {
    CostBreakdown breakdownA, breakdownB;
    bool validA = calculateCost(connA, breakdownA);
    bool validB = calculateCost(connB, breakdownB);

    if (!validA || !validB) {
        std::cerr << "[CostCalculator Error] Cannot compare routes due to invalid input parameters.\n";
        return;
    }

    std::cout << "\n==================================================================================\n";
    std::cout << "                      ROUTE CONNECTION COST COMPARISON                            \n";
    std::cout << "==================================================================================\n";
    
    std::cout << "\n[Route A: " << connA.regionName << " -> " << connA.serverName << "]\n";
    printCostBreakdown(connA, breakdownA);

    std::cout << "\n[Route B: " << connB.regionName << " -> " << connB.serverName << "]\n";
    printCostBreakdown(connB, breakdownB);

    std::cout << "\n--- [Cost Optimization Decision] ---\n";
    if (breakdownA.totalContinuousCost < breakdownB.totalContinuousCost) {
        double delta = breakdownB.totalContinuousCost - breakdownA.totalContinuousCost;
        std::cout << "  ==> OPTIMAL ROUTE: Route A [" << connA.regionName << " -> " << connA.serverName 
                  << "] is CHEAPER by " << std::fixed << std::setprecision(2) << delta 
                  << " cost units (" 
                  << std::setprecision(1) << (delta / breakdownB.totalContinuousCost * 100.0) 
                  << "% savings).\n";
    } else if (breakdownB.totalContinuousCost < breakdownA.totalContinuousCost) {
        double delta = breakdownA.totalContinuousCost - breakdownB.totalContinuousCost;
        std::cout << "  ==> OPTIMAL ROUTE: Route B [" << connB.regionName << " -> " << connB.serverName 
                  << "] is CHEAPER by " << std::fixed << std::setprecision(2) << delta 
                  << " cost units (" 
                  << std::setprecision(1) << (delta / breakdownA.totalContinuousCost * 100.0) 
                  << "% savings).\n";
    } else {
        std::cout << "  ==> BOTH ROUTES HAVE IDENTICAL COST: " 
                  << breakdownA.totalContinuousCost << " cost units.\n";
    }
    std::cout << "==================================================================================\n\n";
}

