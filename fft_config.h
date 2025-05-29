#ifndef FFT_CONFIG_H
#define FFT_CONFIG_H

#include <stdint.h>

//  constants shared across project
#define TIME_BUFFER_SIZE_DEFAULT 2048
#define MAX_BUFFER_LIMIT     10000000
#define EPSILON   1e-12

#define FFT_SIZE 4096
#define FFT_BINS (FFT_SIZE / 2 + 1)

//  Declarations from C++ FFT_Thread.cpp
void start_fft_stream();
void set_fft_mode(int mode);
void get_fft_magnitudes(double *dst, int count);

//  Declarations from C/C++ TimeD_Thread.cpp
#ifdef __cplusplus
extern "C" {
#endif

void set_time_buffer_size(int size);
void start_time_stream(void);
int  get_time_sample_count(void);
void get_time_domain_buffer(uint16_t *dst, int count);

#ifdef __cplusplus
}
#endif

#endif /* FFT_CONFIG_H */
