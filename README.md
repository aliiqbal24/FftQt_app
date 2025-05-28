# FFTQt_app

A real-time signal visualization app built with **Qt 6**, **Qwt**, and powered by **Intel MKL** + **FFTW** for high-performance FFT analysis. Designed for use with the **DPD80 photodetector** by Resolved Instruments.

---

## 📡 Features

- 📈 **Dual plot view**: Time-domain + Frequency-domain
- 🧠 **Real-time FFT processing** using MKL + FFTW
- 🎚️ Adjustable modes: Full bandwidth / Low bandwidth
- ⏸️ **Pause/play** real-time updates
- 💾 Save plots as `.txt`, `.csv`
- 🧱 Clean Qt UI with responsive layout

---

## 🛠️ Build Instructions

### ⚙️ Requirements

- Qt 6.9 (MinGW 64-bit)
- Qwt 6.3.0 (built with Qt 6)
- Intel MKL (2025 or later)
- FFTW3
- libri SDK (from Resolved Instruments)

### 🔧 Setup

1. Clone the repo:
   ```bash
   git clone https://github.com/aliiqbal24/FftQt_app.git
