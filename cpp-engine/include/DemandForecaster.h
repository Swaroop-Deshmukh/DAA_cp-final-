#ifndef DEMAND_FORECASTER_H
#define DEMAND_FORECASTER_H

#include <vector>
#include <string>
#include <iostream>

/**
 * @brief Output container for Holt's Linear Exponential Smoothing forecasting.
 */
struct ForecastResult {
    double finalLevel;                      // Latest estimated base level L_t
    double finalTrend;                      // Latest estimated trend slope T_t
    std::vector<double> fittedValues;       // In-sample smoothed estimates
    std::vector<double> futureForecasts;    // Extrapolated predictions for steps t + 1, t + 2, ..., t + m
};

/**
 * @brief Holt's Linear Exponential Smoothing Demand Predictor.
 * 
 * Mathematical Formulation:
 * -------------------------
 * Holt's two-parameter linear exponential smoothing extends simple exponential
 * smoothing by explicitly modeling linear trends in time-series data.
 * 
 * Equations:
 * 1. Base Level Update (L_t):
 *      L_t = alpha * y_t + (1 - alpha) * (L_{t-1} + T_{t-1})
 *    where:
 *      - alpha in [0, 1]: Smoothing parameter for level
 *      - y_t: Observed traffic demand at time t
 *      - L_{t-1} + T_{t-1}: Previous one-step-ahead estimate
 * 
 * 2. Trend Slope Update (T_t):
 *      T_t = beta * (L_t - L_{t-1}) + (1 - beta) * T_{t-1}
 *    where:
 *      - beta in [0, 1]: Smoothing parameter for trend
 *      - L_t - L_{t-1}: Instantaneous change in level
 * 
 * 3. Forecast Equation (m steps into the future):
 *      y_hat_{t+m} = L_t + m * T_t
 * 
 * Initialization (for t = 2):
 *    L_2 = y_2
 *    T_2 = y_2 - y_1
 */
class DemandForecaster {
private:
    double alpha;   // Level smoothing coefficient [0.0, 1.0] (default: 0.3)
    double beta;    // Trend smoothing coefficient [0.0, 1.0] (default: 0.1)

public:
    DemandForecaster(double alphaWeight = 0.3, double betaWeight = 0.1);

    bool setAlpha(double a);
    bool setBeta(double b);

    double getAlpha() const;
    double getBeta() const;

    /**
     * @brief Computes Holt's linear smoothing over historical traffic demands and forecasts m steps.
     * @param history Sequence of observed historical demands (req/s).
     * @param forecastSteps Number of future intervals m to predict (e.g., 3).
     * @return ForecastResult containing final level, trend, and extrapolated demands.
     */
    ForecastResult forecast(const std::vector<double>& history, int forecastSteps = 3) const;

    /**
     * @brief Formats and displays a forecast summary table.
     */
    static void printForecastSummary(const std::vector<double>& history, const ForecastResult& result);
};

#endif // DEMAND_FORECASTER_H

