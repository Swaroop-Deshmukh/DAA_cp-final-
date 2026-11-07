#include "../include/DemandForecaster.h"
#include <iostream>
#include <iomanip>
#include <cmath>

DemandForecaster::DemandForecaster(double alphaWeight, double betaWeight)
    : alpha(alphaWeight), beta(betaWeight) {
    if (alpha < 0.0 || alpha > 1.0) alpha = 0.3;
    if (beta < 0.0 || beta > 1.0) beta = 0.1;
}

bool DemandForecaster::setAlpha(double a) {
    if (a < 0.0 || a > 1.0) {
        std::cerr << "[DemandForecaster Error] Alpha must be in [0.0, 1.0] (" << a << ").\n";
        return false;
    }
    alpha = a;
    return true;
}

bool DemandForecaster::setBeta(double b) {
    if (b < 0.0 || b > 1.0) {
        std::cerr << "[DemandForecaster Error] Beta must be in [0.0, 1.0] (" << b << ").\n";
        return false;
    }
    beta = b;
    return true;
}

double DemandForecaster::getAlpha() const { return alpha; }
double DemandForecaster::getBeta() const { return beta; }

ForecastResult DemandForecaster::forecast(const std::vector<double>& history, int forecastSteps) const {
    ForecastResult result;
    result.finalLevel = 0.0;
    result.finalTrend = 0.0;

    int n = static_cast<int>(history.size());
    if (n == 0) return result;

    if (n == 1) {
        result.finalLevel = history[0];
        result.finalTrend = 0.0;
        result.fittedValues.push_back(history[0]);
        for (int m = 1; m <= forecastSteps; ++m) {
            result.futureForecasts.push_back(history[0]);
        }
        return result;
    }

    // Initialization:
    // Base level L_1 and initial slope T_1 from the first two observations
    double L = history[0];
    double T = history[1] - history[0];

    result.fittedValues.push_back(L);

    // Iterative smoothing updates for t = 1 to n - 1
    for (int t = 1; t < n; ++t) {
        double y_t = history[t];
        double L_prev = L;
        double T_prev = T;

        // Level Equation: L_t = alpha * y_t + (1 - alpha) * (L_{t-1} + T_{t-1})
        L = alpha * y_t + (1.0 - alpha) * (L_prev + T_prev);

        // Trend Equation: T_t = beta * (L_t - L_{t-1}) + (1 - beta) * T_{t-1}
        T = beta * (L - L_prev) + (1.0 - beta) * T_prev;

        result.fittedValues.push_back(L);
    }

    result.finalLevel = L;
    result.finalTrend = T;

    // Forecast Equation: y_hat_{t+m} = L_t + m * T_t
    for (int m = 1; m <= forecastSteps; ++m) {
        double pred = L + static_cast<double>(m) * T;
        // Traffic demands cannot be negative
        if (pred < 0.0) pred = 0.0;
        result.futureForecasts.push_back(pred);
    }

    return result;
}

void DemandForecaster::printForecastSummary(const std::vector<double>& history, const ForecastResult& result) {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 HOLT LINEAR EXPONENTIAL SMOOTHING FORECAST                             \n";
    std::cout << "========================================================================================\n";
    std::cout << "  Historical Periods (T) : " << history.size() << " time intervals\n";
    std::cout << "  Final Base Level (L_t) : " << std::fixed << std::setprecision(1) << result.finalLevel << " req/s\n";
    std::cout << "  Estimated Trend (T_t)  : " << (result.finalTrend >= 0 ? "+" : "") 
              << std::fixed << std::setprecision(2) << result.finalTrend << " req/s per interval\n\n";

    std::cout << "--- [Historical Observations vs. Smoothed Fit] ---\n";
    std::cout << "+---------------+--------------------+--------------------+\n";
    std::cout << "| Period        | Observed (req/s)   | Smoothed Fit       |\n";
    std::cout << "+---------------+--------------------+--------------------+\n";
    for (size_t t = 0; t < history.size(); ++t) {
        std::cout << "| t = " << std::left << std::setw(9) << (t + 1)
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << history[t]
                  << " | " << std::setw(18) << std::fixed << std::setprecision(1) << result.fittedValues[t] << " |\n";
    }
    std::cout << "+---------------+--------------------+--------------------+\n";

    std::cout << "\n--- [Extrapolated Future Demand Forecast] ---\n";
    std::cout << "+---------------+--------------------+--------------------+\n";
    std::cout << "| Step (Ahead)  | Forecast (req/s)   | Projected Shift    |\n";
    std::cout << "+---------------+--------------------+--------------------+\n";
    for (size_t m = 0; m < result.futureForecasts.size(); ++m) {
        double shift = result.futureForecasts[m] - result.finalLevel;
        std::cout << "| t + " << std::left << std::setw(9) << (m + 1)
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << result.futureForecasts[m]
                  << " | " << std::setw(13) << (shift >= 0 ? "+" : "") << std::fixed << std::setprecision(1) << shift << " req/s |\n";
    }
    std::cout << "+---------------+--------------------+--------------------+\n";
    std::cout << "========================================================================================\n\n";
}

