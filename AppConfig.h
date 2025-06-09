// AppConfig.h
#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <cstddef>

struct AppConfig {
    static inline double sampleRate   = 80e6;

    static inline double timeWindowSeconds  = 100e-6;// semi adjustable

    static inline int maxPointsToPlot  = 10000; // reconsider this number

    static inline int plotRefreshRateMs  = 5; // lower the better tbh, but theres better ways to improve responsiveness

    static inline int fftSize = 19683; // look into optimization ( bluestines, and primes)
    // read https://www.intel.com/content/www/us/en/developer/articles/release-notes/onemkl-release-notes.html
    // talks about why we couldn't do even size

    static inline int fftBins = fftSize / 2 + 1;

    static inline constexpr double fftOverlapFraction = 0.5; // overlapping fft windows, gpts idea ( starts when 50% buffer is full)
    static inline int fftHopSize = static_cast<int>(fftSize * (1.0 - fftOverlapFraction)); // speeds up fft in small bandwidth

    static constexpr double epsilon = 1e-12;

    // signal val to uW conversion - double check this conversion
    static inline double adcOffset = 49555.0;
    static inline double adcToMicroWatts   = 0.0147;

    // Toggle Hilbert-based envelope normalization of each FFT window
    static inline bool envelopeFilterEnabled = true;

};

#endif // APPCONFIG_H
