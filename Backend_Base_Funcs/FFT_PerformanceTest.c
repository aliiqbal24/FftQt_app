// This code to confirm enough FFTS/sec. due to threading and buffering we are sitting at 19k+, enough for no data loss

// realtime di

// BEOFRE RUNNING, USE THIS COMMAND
// IMPORTANT: $ export PATH="$PATH:/c/Program Files (x86)/Intel/oneAPI/mkl/latest/bin" // I have set up in my path

// Double buffered code for input collection, inp
// threaded FFT for optimization, 3 threads, 8 buffers is ideal
#include "ri.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "math.h"
#include "mkl.h"
#include "fftw3.h"
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h> // for sleep() on POSIX

#define FFT_SIZE 4096
#define NUM_FFT_THREADS 3 // stops helping at 3, no more benefit increase
#define NUM_BUFFERS 8     // 8 buffers is ideal for 3 thread, should be 2-3X threads

// 3 threads is upper limit.

// Buffer pool
static double fft_buffers[NUM_BUFFERS][FFT_SIZE];
static int write_index = 0;

// Queue for FFT threads
static double *fft_queue[NUM_BUFFERS];
static int queue_head = 0;
static int queue_tail = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

// Data buffer filling
static double *current_buffer = fft_buffers[0];
static int buffer_index = 0;

// Stats
static atomic_int fft_executions = 0;
static atomic_int fft_counter = 0;

// FFT Thread Function
void *fft_thread_func(void *arg)
{
    double *fft_input;
    fftw_complex fft_output[FFT_SIZE / 2 + 1];
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, NULL, fft_output, FFTW_ESTIMATE);

    while (1)
    {
        pthread_mutex_lock(&queue_mutex);
        while (queue_head == queue_tail)
            pthread_cond_wait(&queue_not_empty, &queue_mutex);

        fft_input = fft_queue[queue_head];
        queue_head = (queue_head + 1) % NUM_BUFFERS;
        pthread_mutex_unlock(&queue_mutex);

        // Re-bind plan input
        fftw_execute_dft_r2c(plan, fft_input, fft_output);
        atomic_fetch_add(&fft_executions, 1);

        if (atomic_fetch_add(&fft_counter, 1) % 100 == 0)
        {
            for (int j = 0; j < FFT_SIZE / 2 + 1; j++)
            {
                if (j != 0 && j % 100 == 0)
                {
                    double re = fft_output[j][0];
                    double im = fft_output[j][1];
                    double mag = sqrt(re * re + im * im);
                    double now_time = (double)clock() / CLOCKS_PER_SEC;
                    // printf("Bin %d: Magnitude: %.2f\n", j, mag);
                }
            }
        }
    }

    return NULL;
}
// ===========================
// Transfer Callback
// ===========================
int transfer_callback(uint16_t *data, int ndata, int dataloss, void *userdata)
{
    if (dataloss)
        printf("[DEBUG] Data loss detected\n");

    for (int i = 0; i < ndata; i++)
    {
        current_buffer[buffer_index++] = (double)data[i];

        if (buffer_index >= FFT_SIZE)
        {
            pthread_mutex_lock(&queue_mutex);
            fft_queue[queue_tail] = current_buffer;
            queue_tail = (queue_tail + 1) % NUM_BUFFERS;
            pthread_cond_signal(&queue_not_empty);
            pthread_mutex_unlock(&queue_mutex);

            write_index = (write_index + 1) % NUM_BUFFERS;
            current_buffer = fft_buffers[write_index];
            buffer_index = 0;
        }
    }

    return 1;
}

// ===========================
// Stats Monitor Thread
// ===========================
void *fft_monitor_thread(void *arg)
{
    while (1) // always running, check fft count, compare second later, difference is FFT/sec
    {
        int prev = fft_executions;
        sleep(1);
        int curr = fft_executions;
        printf("[STATS] FFTs/sec: %d\n", curr - prev);
    }
    return NULL;
}

// ===========================
// Main
// ===========================
int main(int argc, char *argv[])
{
    ri_init();

    pthread_t fft_threads[NUM_FFT_THREADS];
    for (int i = 0; i < NUM_FFT_THREADS; ++i) // make FFT threads
        pthread_create(&fft_threads[i], NULL, fft_thread_func, NULL);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, fft_monitor_thread, NULL);

    ri_device *device = ri_open_device();
    if (!device)
    {
        printf("unable to find a ri device\n");
        return 1;
    }

    double initial_time = (double)clock() / CLOCKS_PER_SEC;

    int64_t dummy = 0;
    printf("transferring continuously... Press Ctrl+C to stop.\n");
    ri_start_continuous_transfer(device, transfer_callback, &dummy);

    double final_time = (double)clock() / CLOCKS_PER_SEC; // doesn't reach as infinite program
    printf("Session duration: %.2f seconds\n", final_time - initial_time);

    device = ri_close_device(device);
    ri_exit();

    return 0;
}
