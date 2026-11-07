#include "../include/CacheOptimizer.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

ContentItem::ContentItem(int itemId, const std::string& itemName, int size, int value)
    : id(itemId), name(itemName), sizeMB(size), popularityValue(value) {}

CachePlacementResult CacheOptimizer::optimizePlacement(const std::vector<ContentItem>& items, int cacheCapacityMB) {
    CachePlacementResult result;
    result.capacityLimitMB = cacheCapacityMB;
    result.totalPopularityValue = 0;
    result.totalSizeUsedMB = 0;

    if (cacheCapacityMB <= 0 || items.empty()) {
        return result;
    }

    int n = static_cast<int>(items.size());
    int W = cacheCapacityMB;

    // 2D DP Table of size (n + 1) x (W + 1)
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(W + 1, 0));

    // Dynamic Programming computation
    for (int i = 1; i <= n; ++i) {
        int itemSize = items[i - 1].sizeMB;
        int itemVal = items[i - 1].popularityValue;

        for (int w = 0; w <= W; ++w) {
            if (itemSize > w) {
                // Item too large for current capacity budget: omit item
                dp[i][w] = dp[i - 1][w];
            } else {
                // Maximize: choice between omitting item or including it
                dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - itemSize] + itemVal);
            }
        }
    }

    result.dpTable = dp;
    result.totalPopularityValue = dp[n][W];

    // Backtracking to recover optimal set of selected content items
    int remCapacity = W;
    for (int i = n; i >= 1; --i) {
        if (dp[i][remCapacity] != dp[i - 1][remCapacity]) {
            // Item i was included in the optimal knapsack
            result.selectedItems.push_back(items[i - 1]);
            result.totalSizeUsedMB += items[i - 1].sizeMB;
            remCapacity -= items[i - 1].sizeMB;
        }
    }

    std::reverse(result.selectedItems.begin(), result.selectedItems.end());
    return result;
}

CachePlacementResult CacheOptimizer::solveBruteForce(const std::vector<ContentItem>& items, int cacheCapacityMB) {
    CachePlacementResult bestResult;
    bestResult.capacityLimitMB = cacheCapacityMB;
    bestResult.totalPopularityValue = 0;
    bestResult.totalSizeUsedMB = 0;

    int n = static_cast<int>(items.size());
    if (n > 20) {
        std::cerr << "[CacheOptimizer Warning] Brute-force solver limited to N <= 20.\n";
        return bestResult;
    }

    int totalSubsets = 1 << n; // 2^n
    for (int mask = 0; mask < totalSubsets; ++mask) {
        int currentSize = 0;
        int currentValue = 0;
        std::vector<ContentItem> currentSubset;

        for (int i = 0; i < n; ++i) {
            if (mask & (1 << i)) {
                currentSize += items[i].sizeMB;
                currentValue += items[i].popularityValue;
                currentSubset.push_back(items[i]);
            }
        }

        if (currentSize <= cacheCapacityMB && currentValue > bestResult.totalPopularityValue) {
            bestResult.totalPopularityValue = currentValue;
            bestResult.totalSizeUsedMB = currentSize;
            bestResult.selectedItems = currentSubset;
        }
    }

    return bestResult;
}

void CacheOptimizer::printPlacementSummary(const CachePlacementResult& result) {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 0/1 KNAPSACK EDGE CACHE PLACEMENT RESULTS                              \n";
    std::cout << "========================================================================================\n";
    std::cout << "  Cache Capacity Budget  : " << result.capacityLimitMB << " MB\n";
    std::cout << "  Storage Space Utilized : " << result.totalSizeUsedMB << " MB (" 
              << std::fixed << std::setprecision(1) 
              << (result.capacityLimitMB > 0 ? (static_cast<double>(result.totalSizeUsedMB) / result.capacityLimitMB) * 100.0 : 0.0)
              << "% utilization)\n";
    std::cout << "  Total Popularity Value : " << result.totalPopularityValue << " value units\n";
    std::cout << "  Total Assets Cached    : " << result.selectedItems.size() << " files\n\n";

    std::cout << "--- [Selected Cached Web Assets] ---\n";
    std::cout << "+-------+------------------------------------------+--------------------+----------------+\n";
    std::cout << "| ID    | Asset / Content Name                     | Size (MB)          | Popularity Val |\n";
    std::cout << "+-------+------------------------------------------+--------------------+----------------+\n";

    for (const auto& item : result.selectedItems) {
        std::cout << "| " << std::left << std::setw(5) << item.id
                  << " | " << std::setw(40) << item.name
                  << " | " << std::right << std::setw(18) << item.sizeMB
                  << " | " << std::setw(14) << item.popularityValue << " |\n";
    }
    std::cout << "+-------+------------------------------------------+--------------------+----------------+\n";
    std::cout << "========================================================================================\n\n";
}

