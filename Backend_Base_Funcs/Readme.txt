#  Real-Time FFT Pipeline for Photodetector (DPD80) data acquisation

The libaries in this project follow our goals in terms of Licensing
---

## Project Timeline 

This project is structured in **4 progressive steps**, each is a stage in the building for the final real-time FFT visualization in qt

---

### 1. `RiStream_PowerSignal`  
**Objective:** Basic real-time data stream from the DPD80 photodetector.

- Captures raw power signal data from the device.
- Prints values to terminal periodically to avoid lag.
- No FFT or processing — purely for validating streaming and timing.
- Can be extended later for a power vs. time plot (top graph of tools like Pterodactyl).

---

### 2. `MKL_interfaceFFT_bugged`  
**Objective:** First attempt at applying Intel MKL's native DFTI interface for FFTs.

- Demonstrates setup of `DftiCreateDescriptor` and FFT execution.
- **Currently outputs zeroed magnitudes due to a bug in usage.**
- Kept as a fallback or future reference in case FFTW performance becomes a bottleneck.
- Superseded by the more robust FFTW interface (see next step).

---

### 3. `FFT_PerformanceTest`  
**Objective:** Achieve high-performance FFT throughput (19k+ FFT/sec) using FFTW threading. (for size N = 4096)

- Uses ping pon buffer + threading the FFT process to prevent data loss.
- Uses **3 parallel FFT threads** with **8 circular buffers** (ideal config).
- FFTs are performed using the FFTW interface (via MKL).
- Prints performance stats (`FFTs/sec`) once per second.
- Prints some magnitude bins periodically for debug validation.
- Foundational for real-time streaming without dropping samples.

---

### 4. `FFT_realtime_Print`  
**Objective:** Final prep before visualization — FFTs are computed and printed in real-time.

- Same architecture as `FFT_PerformanceTest.c`, but prints out **actual FFT magnitudes**.
- This is the **final step before GUI visualization** (in Qt app).
- Serves as the backend benchmark for plotting frequency-domain signals in real time.

---

## Architecture Decisions

- **Threading and Buffering** were required due to the high data rate of the DPD80.
- **FFTW (included in MKL)** was chosen over MKL’s DFTI because it's easier to use and more flexible, apparently slightly slower, but no issues yet
- **Circular buffers** allow continuous streaming without needing to stop or drop data.
- ** Double buffering + mutex-protected queues** avoid contention between data collection and FFT processing.

---

## Setup Instructions (Windows, MinGW, MKL, libri)

Before compiling, ensure:

1. Intel MKL and libri SDK are installed. latest mkl used
2. MKL's bin path is exported:
   ```bash
   command export PATH="$PATH:/c/Program Files (x86)/Intel/oneAPI/mkl/latest/bin"
   moving the error giving dll from this path might also do
3. You want to use the 64 bit systems, so make sure you have the proper complier (mingw 64)

 
## Possible adjustments

Currently this code fails with FFT sizes that are 2^n greater than 4096, something like 4097 is fine, should be looked into
Buffering logic might struggle with the size, even though its dynamically sized

