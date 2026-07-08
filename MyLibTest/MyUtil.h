#pragma once
#include <sstream>
#include <iomanip>
#include <cmath>
#include <string>

// Utility: shorten large numbers with K/M/G suffix. e.g. 1234 -> "1.2K", 1200000 -> "1.2M".
// `decimals` controls number of fractional digits shown (0..n). Default = 1.
// Handles negative numbers. Caps at 'G' (billions).
// Behavior change: decimals are shown only when the absolute original value > 1000.
// For values <= 1000, the function will return an integer-form string (no fractional digits).
static std::string ShortenNumber(double value, int decimals = 1)
{
    if (std::isnan(value)) return std::string("nan");
    if (std::isinf(value)) return (value < 0) ? std::string("-inf") : std::string("inf");

    const char* suffixes[] = { "", "K", "M", "G" };
    const int maxIndex = 3;

    double originalAbs = std::fabs(value);
    double v = value;
    int idx = 0;
    double absV = std::fabs(v);

    while (absV >= 1000.0 && idx < maxIndex) {
        v /= 1000.0;
        absV = std::fabs(v);
        ++idx;
    }

    // Determine whether to show decimals: only when original absolute value > 1000
    int decimalsToUse = (originalAbs > 1000.0) ? decimals : 0;

    // Round to requested decimals and handle rollover (e.g. 999.95 -> 1000.0 -> escalate suffix)
    if (decimalsToUse >= 0) {
        double scale = std::pow(10.0, decimalsToUse);
        double rounded = std::round(v * scale) / scale;
        if (std::fabs(rounded) >= 1000.0 && idx < maxIndex) {
            rounded /= 1000.0;
            ++idx;
        }
        v = rounded;
    }

    std::ostringstream ss;
    if (decimalsToUse >= 0) {
        ss << std::fixed << std::setprecision(decimalsToUse) << v;
        std::string s = ss.str();
        // trim trailing zeros and optional trailing dot when decimals > 0
        if (decimalsToUse > 0) {
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        s += suffixes[idx];
        return s;
    }
    else {
        // if decimals < 0 treat as no fractional digits
        ss << std::fixed << std::setprecision(0) << v;
        std::string s = ss.str();
        s += suffixes[idx];
        return s;
    }
}
