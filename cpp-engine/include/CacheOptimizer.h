#ifndef CACHE_OPTIMIZER_H
#define CACHE_OPTIMIZER_H

#include <string>
#include <vector>
#include <iostream>

/**
 * @brief Represents a web asset / content item candidate for edge cache placement.
 */
struct ContentItem {
    int id;                     // Unique content identifier
    std::string name;           // Content name / URI (e.g. "hero-video.mp4", "app-bundle.js")
    int sizeMB;                 // Storage footprint in MB (Weight w_i in 0/1 Knapsack)
    int popularityValue;        // Expected cache hits / bandwidth savings value (Value v_i in 0/1 Knapsack)

    ContentItem(int itemId, const std::string& itemName, int size, int value);
};

/**
 * @brief Output result of the 0/1 Knapsack Cache Optimizer.
 */
struct CachePlacementResult {
    int totalPopularityValue;               // Maximum aggregate cache value achieved
    int totalSizeUsedMB;                    // Total cache storage consumed
    int capacityLimitMB;                    // Configured edge cache storage limit
    std::vector<ContentItem> selectedItems; // Optimal subset of items cached
    std::vector<std::vector<int>> dpTable;  // 2D DP memoization table for educational inspection
};

/**
 * @brief Dynamic Programming 0/1 Knapsack Cache Placement Optimizer.
 * 
 * Algorithmic Formulation:
 * ------------------------
 * Let N be the number of content items and W be the cache capacity (in MB).
 * DP State:
 *   dp[i][w] = Maximum popularity value achievable considering the first i items
 *              with a storage budget of w MB.
 * 
 * Recurrence Relation:
 *   If size[i] > w:
 *       dp[i][w] = dp[i-1][w]
 *   Else:
 *       dp[i][w] = max(dp[i-1][w], dp[i-1][w - size[i]] + value[i])
 * 
 * Base Case:
 *   dp[0][w] = 0 for all w
 *   dp[i][0] = 0 for all i
 * 
 * Complexity:
 *   Time:  O(N * W) pseudo-polynomial
 *   Space: O(N * W) for full table with optimal backtracking
 */
class CacheOptimizer {
public:
    /**
     * @brief Computes the optimal subset of content items to place in edge cache.
     * @param items Vector of candidate content items.
     * @param cacheCapacityMB Edge cache capacity limit in MB.
     * @return CachePlacementResult containing optimal items, total value, and DP table.
     */
    static CachePlacementResult optimizePlacement(const std::vector<ContentItem>& items, int cacheCapacityMB);

    /**
     * @brief Brute-force solver (exponential O(2^N)) for testing and ground-truth validation on small inputs.
     */
    static CachePlacementResult solveBruteForce(const std::vector<ContentItem>& items, int cacheCapacityMB);

    /**
     * @brief Formats and displays the cache placement decision table.
     */
    static void printPlacementSummary(const CachePlacementResult& result);
};

#endif // CACHE_OPTIMIZER_H

