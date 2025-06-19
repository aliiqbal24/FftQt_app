#ifndef TIMEDPROCESS_H
#define TIMEDPROCESS_H

#include <QObject>
#include <QThread>
#include <stdint.h>

/*!
 * Handles the circular buffer used for the time‑domain plot.
 * It no longer touches the RI device – samples arrive via the
 * static transferCallback forwarded from FFTProcess.
 */
class TimeDProcess : public QObject
{
    Q_OBJECT
public:
    explicit TimeDProcess(QObject *parent = nullptr);
    ~TimeDProcess();

    void start();                                // start worker thread
    bool isRunning() const { return workerThread.isRunning(); }
    void resize(int size);                       // resize circular buffer
    int  sampleCount() const;                    // total collected samples
    void getBuffer(uint16_t *dst, int count);    // copy ‘count’ samples to dst

    static TimeDProcess* instance;

    // Called by FFTProcess’ USB callback to feed fresh ADC words
    static int transferCallback(uint16_t *data, int ndata, int dataloss, void *user);

private:
    QThread workerThread;
    bool started;  // instance-level flag to prevent duplicate starts
    std::atomic<bool> isResizing = false;  // testing line

};

#endif // TIMEDPROCESS_H
