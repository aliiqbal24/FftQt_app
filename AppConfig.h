// AppConfig.h
#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <cstddef>

struct AppConfig {
    static inline double sampleRate   = 80e6;
    static inline double timeWindowSeconds  = 100e-6;
    static inline int maxPointsToPlot   = 10000;
    static inline int plotRefreshRateMs  = 25; // lower the better tbh, but theres better ways to improve responsiveness

    static inline int fftSize  = 16385;
    static inline int fftBins  = fftSize / 2 + 1;

    static constexpr double epsilon = 1e-12;

    static inline double adcOffset = 49555.0;
    static inline double adcToMicroWatts   = 0.0147;
};

#endif // APPCONFIG_H
