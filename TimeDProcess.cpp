#include "TimeDProcess.h"
#include "AppConfig.h"

#include <pthread.h>
#include <QDebug>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <atomic>

namespace {
// buffer size = sample‑rate × time‑window
static int  dynamic_time_buffer_size = AppConfig::timeWindowSeconds * AppConfig::sampleRate;

static uint16_t   *time_buffer   = nullptr;
static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
static int   total_samples_collected = 0;
static int write_idx = 0;
}

TimeDProcess* TimeDProcess::instance = nullptr;  // Static instance pointer

TimeDProcess::TimeDProcess(QObject *parent)
    : QObject(parent), started(false)
{
    this->moveToThread(&workerThread);
    instance = this;  // Set static pointer to current instance
}

TimeDProcess::~TimeDProcess()
{
    workerThread.quit();
    workerThread.wait();

    // fix dangling pointer - could be problematic seems competent for now
    pthread_mutex_lock(&time_mutex);
    uint16_t *old_buffer = time_buffer;
    time_buffer = nullptr;
    dynamic_time_buffer_size = 0;
    total_samples_collected = 0;
    write_idx = 0;  // reset to avoid out-of-bounds writes on next use
    pthread_mutex_unlock(&time_mutex);

    free(old_buffer);

    instance = nullptr;
}

void TimeDProcess::start()
{
    if (started || workerThread.isRunning()) {
        qDebug() << "[TimeDProcess] Already started.";
        return;
    }

    connect(&workerThread, &QThread::started, this, [this]() {
        qDebug() << "[TimeDProcess] Thread started";

        pthread_mutex_lock(&time_mutex);
        if (!time_buffer) {
            pthread_mutex_unlock(&time_mutex);
            resize(dynamic_time_buffer_size);
        } else {
            pthread_mutex_unlock(&time_mutex);
        }
    });

    workerThread.start();
    started = true;
}

void TimeDProcess::resize(int size)
{
    isResizing.store(true);

    pthread_mutex_lock(&time_mutex);

    uint16_t *new_buffer = static_cast<uint16_t *>(calloc(size, sizeof(uint16_t)));
    if (!new_buffer) {
        pthread_mutex_unlock(&time_mutex);
        qWarning("[TimeDProcess] Failed to allocate time buffer");
        isResizing.store(false);
        return;
    }

    uint16_t *old_buffer = time_buffer;
    time_buffer = new_buffer;
    dynamic_time_buffer_size = size;
    total_samples_collected = 0;
    write_idx = 0;  // reset to avoid out-of-bounds writes on resize

    pthread_mutex_unlock(&time_mutex);
    free(old_buffer);

    isResizing.store(false);
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

    int n  = std::min(count, dynamic_time_buffer_size);
    int end = total_samples_collected;
    int start = (end - n + dynamic_time_buffer_size) % dynamic_time_buffer_size;

    for (int i = 0; i < n; ++i)
        dst[i] = time_buffer[(start + i) % dynamic_time_buffer_size];

    pthread_mutex_unlock(&time_mutex);
}

int TimeDProcess::transferCallback(uint16_t *data, int ndata, int /*dataloss*/, void * /*user*/)
{
    if (instance && instance->isResizing.load()) {
        return 0;  // Skip if resizing in progress
    }

    pthread_mutex_lock(&time_mutex);

    if (!time_buffer) {
        pthread_mutex_unlock(&time_mutex);
        return 0;
    }

    for (int i = 0; i < ndata; ++i) {
        time_buffer[write_idx] = data[i];
        write_idx = (write_idx + 1) % dynamic_time_buffer_size;
    }

    total_samples_collected = std::min(total_samples_collected + ndata, dynamic_time_buffer_size);

    pthread_mutex_unlock(&time_mutex);
    return 1;
}
