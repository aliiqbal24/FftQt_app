// attempt with INTEL MKL FFT library, traditional Interface, which is slighlty faster than FFTW interface
// magnitudes are not currently correct, all 0s
// might be helpful if we want to consider speeding up each fft, should not be necessary

#include "ri.h"
#include "stdio.h"
#include "time.h"
#include "math.h"
#include "mkl.h"
#include "mkl_dfti.h"

#define FFT_SIZE 8192 // Can adjust this value for resolution vs speed, start for testing

static double input_buffer[FFT_SIZE];
static MKL_Complex16 output_buffer[FFT_SIZE / 2 + 1]; // output of real-to-complex FFT smaller cuz of FFT
static int buffer_index = 0;

DFTI_DESCRIPTOR_HANDLE handle;

int transfer_callback(uint16_t *data, int ndata, int dataloss, void *userdata)
{
    int64_t *samples_left = (int64_t *)userdata;

    if (dataloss) // built in libri check
        printf("[DEBUG] Data loss detected\n");

    for (int i = 0; i < ndata; i++)
    {
        input_buffer[buffer_index++] = (double)data[i];

        // Process every time a full FFT block is ready
        while (buffer_index >= FFT_SIZE)
        {
            DftiComputeForward(handle, input_buffer, output_buffer);
            DftiSetValue(handle, DFTI_PLACEMENT, DFTI_NOT_INPLACE);

            for (int k = 0; k < FFT_SIZE / 2 + 1; k += 8)
            {
                double real = output_buffer[k].real;
                double imag = output_buffer[k].imag;
                double mag = sqrt(real * real + imag * imag);
                printf("Bin %d: Magnitude = %.30f\n", k, mag); // bin system, can be set to frequency
            }

            fflush(stdout);
            buffer_index = 0; // reset buffer for next FFT block
        }
    }

    *samples_left -= ndata;
    return *samples_left > 0;
}

int main(int argc, char *argv[]) // ERROR HERE, callback is not being called
{
    int err = 0;

    ri_init();

    ri_device *device = 0;

    device = ri_open_device();
    if (device == 0)
    {
        printf("unable to find a ri device\n");
    }
    else
    {
        const int64_t samples_to_transfer = 80 * 1000 * 1000;
        double initial_time = (double)clock() / CLOCKS_PER_SEC;

        int64_t samples_left = samples_to_transfer;

        printf("transferring %lld samples...\n", samples_to_transfer);
        ri_start_continuous_transfer(device, transfer_callback, &samples_left);

        double final_time = (double)clock() / CLOCKS_PER_SEC;
        double MBs_transferred = samples_to_transfer * 2. / 1000000.;
        printf("transfered %.1f MB in %g s of cpu time\n", MBs_transferred, final_time - initial_time);
        printf("%.2f MBPS", MBs_transferred / (final_time - initial_time));

        device = ri_close_device(device);

        DftiFreeDescriptor(&handle); // free at end of main, should be good
    }

    ri_exit();

#ifdef _MSC_VER
    // if Visual Studio get keyboard input at end of program so it doesn't automatically close
    getchar();
#endif
    return 0;
}
