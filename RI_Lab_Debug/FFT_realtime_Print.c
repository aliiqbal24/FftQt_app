// IMPORTANT, to run this program, must first use command$ export PATH="$PATH:/c/Program Files (x86)/Intel/oneAPI/mkl/latest/bin" // this can be setup as path in enviromental variables
// this might also be fixed by including dll in directory
// This the program that the plotting is built on, and has the same FFTs/sec as FFT/sec.c as it follows that process but with display. no monitoring

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
#define NUM_FFT_THREADS 3
#define NUM_BUFFERS 8

static double fft_buffers[NUM_BUFFERS][FFT_SIZE];
static int write_index = 0;

static double *fft_queue[NUM_BUFFERS];
static int queue_head = 0;
static int queue_tail = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

static double *current_buffer = fft_buffers[0];
static int buffer_index = 0;

void *fft_thread_func(void *arg) // called
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

        fftw_execute_dft_r2c(plan, fft_input, fft_output);

        for (int j = 0; j < FFT_SIZE / 2 + 1; j++)
        {
            if (j != 0 && j % 100 == 0) // Do this every 100th, mainly so that print statements are realtime, for plotting this is every iteration, 0 bin is high so muted for now
            {
                double re = fft_output[j][0];
                double im = fft_output[j][1];
                double mag = sqrt(re * re + im * im);
                printf("Bin %d: Magnitude: %.2f\n", j, mag); // in ploting we do freq, not bins
            }
        }
    }

    return NULL;
}

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

int main(int argc, char *argv[])
{
    ri_init();

    pthread_t fft_threads[NUM_FFT_THREADS];
    for (int i = 0; i < NUM_FFT_THREADS; ++i)
        pthread_create(&fft_threads[i], NULL, fft_thread_func, NULL);

    ri_device *device = ri_open_device();
    if (!device)
    {
        printf("unable to find a ri device\n");
        return 1;
    }

    int64_t dummy = 0;
    printf("transferring continuously... Press Ctrl+C to stop.\n");
    ri_start_continuous_transfer(device, transfer_callback, &dummy);

    device = ri_close_device(device);
    ri_exit();

    return 0;
}
