#include "TimeDProcess.h"
#include "AppConfig.h"

#include <pthread.h>
#include <QDebug>
#include <algorithm>
#include <stdlib.h>
#include <string.h>

namespace {
// buffer size = sample‑rate × time‑window
static int         dynamic_time_buffer_size =
    AppConfig::timeWindowSeconds * AppConfig::sampleRate;

static uint16_t   *time_buffer   = nullptr;
static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
static int         total_samples_collected = 0;
} // unnamed namespace

TimeDProcess::TimeDProcess(QObject *parent)
    : QObject(parent)
{
    this->moveToThread(&workerThread);
}

TimeDProcess::~TimeDProcess()
{
    workerThread.quit();
    workerThread.wait();
}

void TimeDProcess::start()
{
    if (workerThread.isRunning())
        return;

    connect(&workerThread, &QThread::started, this, [this]() {
        qDebug() << "[TimeDProcess] Thread started";

        // allocate buffer once, if not already done
        if (!time_buffer)
            resize(dynamic_time_buffer_size);

        // no device interaction here – samples arrive via transferCallback
    });

    workerThread.start();
}

void TimeDProcess::resize(int size)
{
    pthread_mutex_lock(&time_mutex);

    uint16_t *new_buffer =
        static_cast<uint16_t *>(calloc(size, sizeof(uint16_t)));
    if (!new_buffer) {
        pthread_mutex_unlock(&time_mutex);
        qWarning("[TimeDProcess] Failed to allocate time buffer");
        return;
    }

    uint16_t *old_buffer = time_buffer;
    time_buffer               = new_buffer;
    dynamic_time_buffer_size  = size;
    total_samples_collected   = 0;

    pthread_mutex_unlock(&time_mutex);
    free(old_buffer);
}

int TimeDProcess::sampleCount() const
{
    pthread_mutex_lock(&time_mutex);
    int result = total_samples_collected;
    pthread_mutex_unlock(&time_mutex);
    return result;
}

void TimeDProcess::getBuffer(uint16_t *dst, int count)
{
    if (!dst || count <= 0)
        return;

    pthread_mutex_lock(&time_mutex);

    if (!time_buffer) {
        pthread_mutex_unlock(&time_mutex);
        memset(dst, 0, count * sizeof(uint16_t));
        return;
    }

    int n     = std::min(count, dynamic_time_buffer_size);
    int end   = total_samples_collected;
    int start = (end - n + dynamic_time_buffer_size) % dynamic_time_buffer_size;

    for (int i = 0; i < n; ++i)
        dst[i] = time_buffer[(start + i) % dynamic_time_buffer_size];

    pthread_mutex_unlock(&time_mutex);
}

// ────────────────────────────────────────────────────────────────
// This is invoked from FFTProcess’ USB callback to enqueue samples
// ────────────────────────────────────────────────────────────────
int TimeDProcess::transferCallback(uint16_t *data,
                                   int ndata,
                                   int /*dataloss*/,
                                   void * /*user*/)
{
    static int write_idx = 0;

    pthread_mutex_lock(&time_mutex);

    if (!time_buffer) {
        pthread_mutex_unlock(&time_mutex);
        return 0;
    }

    for (int i = 0; i < ndata; ++i) {
        time_buffer[write_idx] = data[i];
        write_idx = (write_idx + 1) % dynamic_time_buffer_size;
    }

    total_samples_collected =
        std::min(total_samples_collected + ndata,
                 dynamic_time_buffer_size);

    pthread_mutex_unlock(&time_mutex);
    return 1;
}
