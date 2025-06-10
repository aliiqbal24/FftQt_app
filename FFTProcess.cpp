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
#include <algorithm> // try to implement filter


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

static double* current_buffer = fft_buffers[0].data();
static int buffer_index = 0;

static FFTMode internalMode = FFTMode::FullBandwidth;
static FFTProcess* fft_instance = nullptr;
static PeakFrequencyCallback peak_callback = nullptr;


// new envelope filter Hillbert function:
static void envelope_filter(double* buffer)
{
    if (!AppConfig::envelopeFilterEnabled)
        return;

    static bool init = false;
    static fftw_complex* hilbert_time = nullptr;
    static fftw_complex* hilbert_freq = nullptr;
    static fftw_plan fwdPlan;
    static fftw_plan invPlan;
    static std::vector<double> amplitude(AppConfig::fftSize);
    static double env_avg = 0.0;

    if (!init) {
        hilbert_time = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * AppConfig::fftSize);
        hilbert_freq = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * AppConfig::fftSize);
        fwdPlan = fftw_plan_dft_1d(AppConfig::fftSize, hilbert_time, hilbert_freq, FFTW_FORWARD, FFTW_ESTIMATE);
        invPlan = fftw_plan_dft_1d(AppConfig::fftSize, hilbert_freq, hilbert_time, FFTW_BACKWARD, FFTW_ESTIMATE);
        init = true;
    }

    for (int i = 0; i < AppConfig::fftSize; ++i) {
        hilbert_time[i][0] = buffer[i]; // real part
        hilbert_time[i][1] = 0.0;    // imaginary part
    }

    fftw_execute(fwdPlan); // fft perform


    // CREATE analytical signal
    int N = AppConfig::fftSize;
    int hN = N >> 1;
    int numRem = hN;

    for (int k = 1; k < hN; ++k) {
        hilbert_freq[k][0] *= 2.0;
        hilbert_freq[k][1] *= 2.0;
    }

    if (N % 2 == 0)
        numRem--;
    else if (N > 1) {
        hilbert_freq[hN][0] *= 2.0;
        hilbert_freq[hN][1] *= 2.0;
    }

    memset(hilbert_freq + hN + 1, 0, numRem * sizeof(fftw_complex));

    fftw_execute(invPlan); // inverse back to time domain

    for (int i = 0; i < N; ++i) {  // compute amplitude
        hilbert_time[i][0] /= N;
        hilbert_time[i][1] /= N;
        amplitude[i] = std::sqrt(hilbert_time[i][0] * hilbert_time[i][0] +
                                 hilbert_time[i][1] * hilbert_time[i][1]);
    }

    for (int i = 0; i < N; ++i) { // filter it outs
        env_avg = 0.999 * env_avg + 0.001 * amplitude[i]; // smoothened estimate
        double scale = (env_avg > 1e-12) ? env_avg : 1.0; // if large enough, use else, fall back to 1
        buffer[i] /= scale; // divide by calculated scale
    }
}


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
            fft_magnitude_buffer[j] = mag;

            if (j >= ignoreBins && j <= ignoreBinsTop && mag > peakValue) {
                peakValue = mag;
                peakIndex = j;
            }
        }

        if (peak_callback) {
            double freq = (peakIndex * ((internalMode == FFTMode::LowBandwidth) ? 200000.0 : 80000000.0)) /AppConfig::fftSize;
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

            if (AppConfig::envelopeFilterEnabled)
                envelope_filter(current_buffer);

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

    for (int i = 0; i < count && i < AppConfig::fftBins; ++i)
        dst[i] = fft_magnitude_buffer[i];

    return true;
}

void FFTProcess::setMode(FFTMode mode)
{
    currentMode = mode;
    internalMode = mode;
}
