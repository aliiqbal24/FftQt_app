
// This code is just to collect data from the photodetector, no FFT here
// Would be useful for a power, time graph, like the top graph in pteradactyl, can be plotted with little change

#include "ri.h"
#include "stdio.h"
#include "time.h"
#include "math.h"
#include "mkl.h"

int transfer_callback(uint16_t *data, int ndata, int dataloss, void *userdata)
{

    int64_t *samples_left = (int64_t *)userdata;

    if (dataloss) // built in libri check
        printf("data loss detected\n");

    // Print each value in the data array, stop when we reach the end of the array
    for (int i = 0; i < ndata; i++)
    {
        if (i % 10000 == 0) // this mod keeps the prints realtime, if we don't then they lag behind
        {
            printf("data[%d] = %u\n", i, data[i]);
        }
    }

    *samples_left -= ndata;
    return *samples_left > 0;
}

int main(int argc, char *argv[]) // Program starts here, initailze device and call continuos transfer
{

    int err = 0; // from example, not used

    ri_init();

    ri_device *device = 0;

    device = ri_open_device();
    if (device == 0) // device not found
    {
        printf("unable to find a ri device\n");
    }
    else // next process is mainly to confirm data transfer
    {
        const int64_t samples_to_transfer = 80 * 1000 * 1000; // this makes the data limited, from the libri example, to be continous adjust logic
        double initial_time = (double)clock() / CLOCKS_PER_SEC;

        int64_t samples_left = samples_to_transfer;

        printf("transferring %lld samples...\n", samples_to_transfer);
        ri_start_continuous_transfer(device, transfer_callback, &samples_left); // function above, print data loop

        double final_time = (double)clock() / CLOCKS_PER_SEC;
        double MBs_transferred = samples_to_transfer * 2. / 1000000.; // these statuses from example, uncecassary for plotting
        printf("transfered %.1f MB in %g s of cpu time\n", MBs_transferred, final_time - initial_time);
        printf("%.2f MBPS", MBs_transferred / (final_time - initial_time));

        device = ri_close_device(device);
    }

    ri_exit();

#ifdef _MSC_VER
    // if Visual Studio get keyboard input at end of program so it doesn't automatically close
    getchar();
#endif
    return 0;
}
