#ifndef TIMEDPROCESS_H
#define TIMEDPROCESS_H

#include <QObject>
#include <QThread>
#include <vector>
#include <cstdint>

// Forward-declare RI device type
struct ri_device;

class TimeDProcess : public QObject {
    Q_OBJECT

public:
    explicit TimeDProcess(QObject *parent = nullptr);
    ~TimeDProcess();

    void start();                                         // Starts streaming
    void resize(int size);                                // Resize circular buffer
    int sampleCount() const;                              // Get number of collected samples
    void getBuffer(uint16_t *dst, int count);             // Copy buffer contents for plotting

    static int transferCallback(uint16_t *data, int ndata, int dataloss, void *user);  // Libri-compatible callback

private:
    QThread workerThread;
    ri_device* dev = nullptr;  // Store device handle for ri_open_device
};

#endif // TIMEDPROCESS_H
