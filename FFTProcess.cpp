// FFTProcess.cpp
#include "FFTProcess.h"
#include "TimeDProcess.h"
#include "AppConfig.h"
#include "ri.h"

#include <pthread.h>
#include <mkl.h>
#include <fftw3.h>
#include <cmath>
#include <QMetaObject>
#include <QApplication>
#include <QThread>
#include <vector>
#include <atomic>
#include <algorithm>

#define NUM_BUFFERS     8
#define NUM_FFT_THREADS 3

using PeakFrequencyCallback = void(*)(double);

// Shared initialization

static std::atomic<int> fft_data_ready{0};
static std::vector<double> fft_magnitude_buffer(AppConfig::fftBins);
static std::vector<std::vector<double>> fft_buffers(NUM_BUFFERS, std::vector<double>(AppConfig::fftSize));

static int write_index = 0;
static double* fft_queue[NUM_BUFFERS];
static std::atomic<int> queue_head{0}, queue_tail{0};
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t magnitude_mutex = PTHREAD_MUTEX_INITIALIZER;

static double* current_buffer = fft_buffers[0].data();
static int buffer_index = 0;

static FFTMode internalMode = FFTMode::FullBandwidth;
static FFTProcess* fft_instance = nullptr;
static PeakFrequencyCallback peak_callback = nullptr;

static void emit_peak(double freq) // refresh peak freq
{
    if (!fft_instance) return;
    QMetaObject::invokeMethod(qApp, [freq]() {
        if (fft_instance)
            fft_instance->peakFrequencyUpdated(freq);
    });
}

static void* fft_thread_func(void*)
{
    fftw_complex* fft_output = new fftw_complex[AppConfig::fftBins];
    fftw_plan plan = fftw_plan_dft_r2c_1d(AppConfig::fftSize, nullptr, fft_output, FFTW_ESTIMATE);
    std::vector<double> local_magnitudes(AppConfig::fftBins);

    while (true) {
        pthread_mutex_lock(&queue_mutex);
        while (queue_head == queue_tail)
            pthread_cond_wait(&queue_not_empty, &queue_mutex);

        double* fft_input = fft_queue[queue_head];
        queue_head = (queue_head + 1) % NUM_BUFFERS;
        pthread_mutex_unlock(&queue_mutex);

        fftw_execute_dft_r2c(plan, fft_input, fft_output);

        int peakIndex = 0;
        double peakValue = 0.0;

        const int ignoreBins = AppConfig::fftBins / 10;
        const int ignoreBinsTop = static_cast<int>(AppConfig::fftBins * 0.99); // ignore spikes at beginning and end

        for (int j = 0; j < AppConfig::fftBins; ++j) {
            double re = fft_output[j][0];
            double im = fft_output[j][1];
            double mag = std::sqrt(re * re + im * im);
            local_magnitudes[j] = mag;

            if (j >= ignoreBins && j <= ignoreBinsTop && mag > peakValue) {
                peakValue = mag;
                peakIndex = j;
            }
        }

        pthread_mutex_lock(&magnitude_mutex);
        std::copy(local_magnitudes.begin(), local_magnitudes.end(), fft_magnitude_buffer.begin());
        pthread_mutex_unlock(&magnitude_mutex);

        if (peak_callback) {
            double freq = (peakIndex *
                           ((internalMode == FFTMode::LowBandwidth) ? 200000.0 : 80000000.0)) /AppConfig::fftSize;
            freq /= (internalMode == FFTMode::LowBandwidth) ? 1000.0 : 1e6;
            peak_callback(freq);
        }

        fft_data_ready.store(1);
    }

    delete[] fft_output;
    return nullptr;
}

static int transfer_callback(uint16_t* data, int ndata, int, void*)
{
    TimeDProcess::transferCallback(data, ndata, 0, nullptr);

    constexpr int ADC_RATE = 80000000;
    int targetRate = (internalMode == FFTMode::LowBandwidth) ? 200000 : ADC_RATE;
    int downsample = ADC_RATE / targetRate;

    static int skip = 0;
    for (int i = 0; i < ndata; ++i) {
        if (skip++ % downsample != 0)
            continue;

        current_buffer[buffer_index++] = static_cast<double>(data[i]);

        if (buffer_index >= AppConfig::fftSize) {
            int next_tail = (queue_tail + 1) % NUM_BUFFERS;
            if (next_tail == queue_head) {
                buffer_index = AppConfig::fftSize - AppConfig::fftHopSize;
                return 1;
            }

            pthread_mutex_lock(&queue_mutex);
            fft_queue[queue_tail] = current_buffer;
            queue_tail = next_tail;
            pthread_cond_signal(&queue_not_empty);
            pthread_mutex_unlock(&queue_mutex);

            write_index = (write_index + 1) % NUM_BUFFERS;
            current_buffer = fft_buffers[write_index].data();

            std::copy(
                fft_buffers[(write_index - 1 + NUM_BUFFERS) % NUM_BUFFERS].data() + AppConfig::fftHopSize,
                fft_buffers[(write_index - 1 + NUM_BUFFERS) % NUM_BUFFERS].data() + AppConfig::fftSize,
                current_buffer
                );

            buffer_index = AppConfig::fftSize - AppConfig::fftHopSize;
        }
    }

    return 1;
}

FFTProcess::FFTProcess(QObject* parent)
    : QObject(parent)
{
    this->moveToThread(&workerThread);

    // Start FFT worker threads only once
    for (int i = 0; i < NUM_FFT_THREADS; ++i)
        pthread_create(&fftThreads[i], nullptr, fft_thread_func, nullptr);
}

FFTProcess::~FFTProcess()
{
    workerThread.quit();
    workerThread.wait();
    // NOTE: You could use pthread_cancel on threads if graceful stop is needed
}

void FFTProcess::start()
{
    if (workerThread.isRunning()) {
        qDebug() << "[FFTProcess] Already started.";
        return;
    }

    // Quick check if the RI device is available before spawning the thread
    ri_init();
    ri_device *probe = ri_open_device();
    if (!probe) {
        qWarning("RI device not found");
        ri_exit();
        return;
    }
    probe = ri_close_device(probe);
    ri_exit();

    connect(&workerThread, &QThread::started, this, [this]() {
        qDebug() << "[FFTProcess] Thread started";

        internalMode = currentMode;
        fft_instance = this;
        peak_callback = emit_peak;

        ri_init();
        ri_device* device = ri_open_device();
        if (!device) {
            qWarning("RI device not found");
            return;
        }

        static int64_t dummy = 0;
        ri_start_continuous_transfer(device, transfer_callback, &dummy);
    });

    workerThread.start();
}

bool FFTProcess::getMagnitudes(double* dst, int count)
{
    if (fft_data_ready.exchange(0) == 0)
        return false;

    pthread_mutex_lock(&magnitude_mutex);
    for (int i = 0; i < count && i < AppConfig::fftBins; ++i)
        dst[i] = fft_magnitude_buffer[i];
    pthread_mutex_unlock(&magnitude_mutex);

    return true;
}

void FFTProcess::setMode(FFTMode mode)
{
    currentMode = mode;
    internalMode = mode;
}
