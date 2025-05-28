#include "ri.h"
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "fft_config.h"

/* ---- ring‑buffer for time‑domain data ---- */
static int dynamic_time_buffer_size = 2048;
static uint16_t *time_buffer        = NULL;
static pthread_mutex_t time_mutex   = PTHREAD_MUTEX_INITIALIZER;
static int total_samples_collected  = 0;

/* resize buffer --------------------------------------------------------*/
void set_time_buffer_size(int size)
{
    pthread_mutex_lock(&time_mutex);
    if (time_buffer) free(time_buffer);
    dynamic_time_buffer_size = size;
    time_buffer = (uint16_t*)malloc(sizeof(uint16_t) * dynamic_time_buffer_size);
    total_samples_collected = 0;
    pthread_mutex_unlock(&time_mutex);
}

/* callback from RI -----------------------------------------------------*/
int time_transfer_callback(uint16_t *data, int ndata, int dataloss, void *userdata)
{
    static int write_idx = 0;
    pthread_mutex_lock(&time_mutex);

    for (int i = 0; i < ndata; ++i) {
        time_buffer[write_idx] = data[i];
        write_idx = (write_idx + 1) % dynamic_time_buffer_size;
    }
    total_samples_collected = (total_samples_collected + ndata > dynamic_time_buffer_size)
                                  ? dynamic_time_buffer_size
                                  : total_samples_collected + ndata;

    pthread_mutex_unlock(&time_mutex);
    return 1;
}

/* data fetch API -------------------------------------------------------*/
void get_time_domain_buffer(uint16_t *dst, int count)
{
    pthread_mutex_lock(&time_mutex);
    int n = (count > dynamic_time_buffer_size) ? dynamic_time_buffer_size : count;
    int end   = total_samples_collected;
    int start = (end - n + dynamic_time_buffer_size) % dynamic_time_buffer_size;
    for (int i = 0; i < n; ++i)
        dst[i] = time_buffer[(start + i) % dynamic_time_buffer_size];
    pthread_mutex_unlock(&time_mutex);
}

int get_time_sample_count()
{
    pthread_mutex_lock(&time_mutex);
    int r = total_samples_collected;
    pthread_mutex_unlock(&time_mutex);
    return r;
}

/* launch ---------------------------------------------------------------*/
void start_time_stream()
{
    if (!time_buffer) set_time_buffer_size(dynamic_time_buffer_size);

    ri_init();
    ri_device *dev = ri_open_device();
    if (!dev) { printf("unable to find a ri device\n"); return; }

    int64_t dummy = 0;
    ri_start_continuous_transfer(dev, time_transfer_callback, &dummy);
    ri_close_device(dev);
    ri_exit();
}
