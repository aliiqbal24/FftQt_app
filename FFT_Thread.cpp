#include "ri.h"
#include "stdio.h"
#include "time.h"
#include "math.h"
#include "mkl.h"
#include "fftw3.h"
#include <pthread.h>
#include <atomic>
#include <unistd.h>

#include "fft_mode.h"
#include "fft_config.h"

#define NUM_FFT_THREADS 3
#define NUM_BUFFERS     8

static double fft_magnitude_buffer[FFT_BINS];
static std::atomic<int> fft_data_ready{0};

static double fft_buffers[NUM_BUFFERS][FFT_SIZE] = {0};
static int    write_index = 0;

static double* fft_queue[NUM_BUFFERS];
static std::atomic<int> queue_head{0}, queue_tail{0}; // atomic should help with thread safety

static pthread_mutex_t queue_mutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  queue_not_empty  = PTHREAD_COND_INITIALIZER;

static double* current_buffer = fft_buffers[0];
static int     buffer_index   = 0;

static FFTMode currentFFTMode = FFTMode::FullBandwidth;


void set_fft_mode(int mode) {
    currentFFTMode = static_cast<FFTMode>(mode);
}

void* fft_thread_func(void* /*arg*/)
{
    double* fft_input;
    fftw_complex fft_output[FFT_BINS];
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, nullptr, fft_output, FFTW_ESTIMATE);

    while (1)
    {
        pthread_mutex_lock(&queue_mutex);
        while (queue_head == queue_tail)
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        fft_input = fft_queue[queue_head];
        queue_head = (queue_head + 1) % NUM_BUFFERS; // ring buffer implementation
        pthread_mutex_unlock(&queue_mutex);

        fftw_execute_dft_r2c(plan, fft_input, fft_output);

        for (int j = 0; j < FFT_BINS; ++j) {
            double re = fft_output[j][0];
            double im = fft_output[j][1];
            fft_magnitude_buffer[j] = sqrt(re * re + im * im);
        }
        fft_data_ready.store(1);
    }

    return nullptr;
}

int transfer_callback(uint16_t* data, int ndata, int dataloss, void* /*userdata*/)
{
    if (!current_buffer) { // prevent empty buffer crash
        current_buffer = fft_buffers[0];  // Fallback to buffer 0
        buffer_index = 0;
    }
    if (dataloss)
        printf("[DEBUG] Data loss detected\n");

    constexpr int ADC_SAMPLE_RATE = 80000000;
    int targetRate;
    if (currentFFTMode == FFTMode::LowBandwidth)
        targetRate = 200000;
    else
        targetRate = ADC_SAMPLE_RATE;

    int downsample_factor = ADC_SAMPLE_RATE / targetRate;
    static int skip_counter = 0;

    static int drop_warn_count = 0;

    for (int i = 0; i < ndata; ++i)
    {
        if (skip_counter++ % downsample_factor != 0) continue;

        if (buffer_index >= FFT_SIZE) {
            // Drop the rest of the samples in this chunk
            if (drop_warn_count < 5) {
                fprintf(stderr, "[WARN] FFT buffer overflow — samples dropped\n");
                ++drop_warn_count;
            }
            break;
        }
        current_buffer[buffer_index++] = static_cast<double>(data[i]);

        if (buffer_index >= FFT_SIZE) {
            pthread_mutex_lock(&queue_mutex);
            fft_queue[queue_tail] = current_buffer;
            queue_tail = (queue_tail + 1) % NUM_BUFFERS;
            pthread_cond_signal(&queue_not_empty);
            pthread_mutex_unlock(&queue_mutex);

            write_index    = (write_index + 1) % NUM_BUFFERS;
            current_buffer = fft_buffers[write_index];
            buffer_index   = 0;
        }
    }
    return 1;
}


void get_fft_magnitudes(double* dst, int count)
{
    if (fft_data_ready.exchange(0) == 0) return;

    for (int i = 0; i < count && i < FFT_BINS; ++i)
        dst[i] = fft_magnitude_buffer[i];
}

void start_fft_stream()
{
    ri_init();

    pthread_t fft_threads[NUM_FFT_THREADS];
    for (int i = 0; i < NUM_FFT_THREADS; ++i)
        pthread_create(&fft_threads[i], nullptr, fft_thread_func, nullptr);

    ri_device* device = ri_open_device();
    if (!device) {
        printf("unable to find a ri device\n");
        return;
    }

    static int64_t dummy = 0;
    printf("transferring continuously... Press Ctrl+C to stop.\n");
    ri_start_continuous_transfer(device, transfer_callback, &dummy);

    // Do NOT call ri_close_device or ri_exit here.
    // The stream must keep running.
}

