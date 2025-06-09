#include "Features.h"
#include "AppConfig.h"

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <cmath>
#include <algorithm>

void Features::togglePause(bool &isPaused) { // flip state
    isPaused = !isPaused;
    qDebug() << "[Features] Pause toggled. isPaused =" << isPaused;
}

// button function
void Features::switchMode(FFTMode &mode) { // mode switch, adjust Psuedo Sample rate
    if (mode == FFTMode::FullBandwidth)
        mode = FFTMode::LowBandwidth;
    else
        mode = FFTMode::FullBandwidth;

    AppConfig::sampleRate = (mode == FFTMode::FullBandwidth) ? 80e6 : 200000 ;

    qDebug() << "[Features] Mode switched to:"<< (mode == FFTMode::FullBandwidth ? "FullBandwidth" : "LowBandwidth");
}

void Features::saveFFTPlot(const QString &fileName,const double *fftBuffer, double sampleRate)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Features] Failed to open file:" << fileName;
        return;
    }

    QTextStream out(&file); // csv headings

    if (sampleRate > 1e6)
        out << "Frequency (MHz),Log Magnitude\n";
    else
        out << "Frequency (KHz),Log Magnitude\n";

    const int bins = AppConfig::fftBins;
    const double eps = AppConfig::epsilon;
    const int fftSize = AppConfig::fftSize;

    for (int i = 0; i < bins; ++i) {   // right to file
        double freq = i * (sampleRate / fftSize);
        freq /= (sampleRate > 1e6) ? 1e6 : 1e3;

        double mag = std::max(fftBuffer[i], eps);
        out << freq << "," << std::log10(mag) << "\n";
    }

    file.close();
    qDebug() << "[Features] FFT plot saved to" << fileName;
}

void Features::saveTimePlot(const QString &fileName,const std::vector<uint16_t> &buffer, double sampleRate, double timeWindowSeconds)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Features] Failed to open file:" << fileName;
        return;
    }

    QTextStream out(&file);
    out << "Time (us),Power (uW)\n";

    const double dt_us = 1e6 / sampleRate;
    const double offset = AppConfig::adcOffset;
    const double scale = AppConfig::adcToMicroWatts;

    for (int i = 0; i < static_cast<int>(buffer.size()); ++i) {
        double time_us = i * dt_us;
        double power_uW = (static_cast<double>(buffer[i]) - offset) * scale; // unreliable scaling
        out << time_us << "," << power_uW << "\n";
    }

    file.close();
    qDebug() << "[Features] Time-domain plot saved to" << fileName;
}

void Features::promptUserToSavePlot(QWidget *parent,const double *fftBuffer,const std::vector<uint16_t> &timeBuffer) // ask user what plot, maybe do this before filenaming?
{
    QSettings settings("Ultracoustics", "RealtimePlotApp");
    QString lastDir = settings.value("lastSavePath", QDir::homePath()).toString();

    QString fileName = QFileDialog::getSaveFileName(parent, "Save Plot Data", lastDir,"Text File (*.txt);;CSV File (*.csv);;Raw Binary (*.raw)");

    if (fileName.isEmpty()) return;
    settings.setValue("lastSavePath", QFileInfo(fileName).absolutePath());

    QString choice = QInputDialog::getItem(parent, "Select Plot", "Choose which Plot data to export:",
    {"Save Time-Domain Plot", "Save Frequency (FFT) Plot"}, 0, false);

    if (choice.isEmpty()) return;

    if (choice == "Save Time-Domain Plot") {
        Features::saveTimePlot(fileName, timeBuffer, AppConfig::sampleRate, AppConfig::timeWindowSeconds);
    } else {
        Features::saveFFTPlot(fileName, fftBuffer, AppConfig::sampleRate);
    }
}

void Features::updatePeakFrequency(QLabel *label, FFTMode mode, double frequency, bool isPaused)  // connected to FFT function, get teh higest magnitude's freq abd display
{
    if (!label) return;
    if (isPaused) return; // stop with plot

    QString unit = (mode == FFTMode::LowBandwidth) ? "kHz" : "MHz";
    QString text = QString("Peak: %1 %2").arg(frequency, 0, 'f', 2).arg(unit);

    label->setText(text);
}
