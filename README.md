# Acoustic Feature Analysis: Reverberation Time (T60) Measurement

## 🇬🇧 Project Overview

This repository provides firmware and a Python post‑processing pipeline
for a dedicated acoustic measurement system designed to measure the
Reverberation Time (T60) of indoor environments using a low‑cost
ESP32‑S3 microcontroller.

The system performs two complementary forms of acoustic data
acquisition:

-   **Active Measurement:** Room Impulse Response (RIR) capture using a
    logarithmic sine sweep (chirp) for precise T60 calculation.\
-   **Passive Measurement:** Ambient pink‑noise recording for acoustic
    environment classification.

All raw audio is saved to an SD card for later batch processing.

------------------------------------------------------------------------

## 🔬 Methodology for T60 Calculation

T60 is computed from the Room Impulse Response using the **Schroeder
integration method**, implemented in `batch_rir_processor.py`.

### 1. RIR Acquisition (Logarithmic Sine Sweep)

A digitally generated sweep is played through a speaker while the MEMS
microphone simultaneously records the resulting room response, capturing
reflections, absorption characteristics, and decay behaviour.

### 2. RIR Extraction (Python)

The clean Room Impulse Response is obtained via **linear
cross‑correlation** between the recorded signal and the original
reference chirp.

### 3. Energy Decay Curve (EDC) + Extrapolation

T60 is defined as the time required for acoustic energy to decay by **60
dB**.\
The script computes the **Energy Decay Curve (EDC)** and fits a linear
regression to multiple decay segments to robustly estimate T60:

-   **T20-based T60:** from −5 dB → −25 dB\
-   **T30-based T60:** from −5 dB → −35 dB\
-   **T25-based T60:** from −5 dB → −30 dB

------------------------------------------------------------------------

## ☁️ Passive Measurement: Pink Noise Recording

This component captures **ambient acoustic data** used for
classification tasks (e.g., identifying whether a recording environment
is a quiet office or a busy café).

### Why Pink Noise?

Pink noise exhibits a **3 dB per octave decrease**, providing consistent
acoustic power across octave bands:

-   More closely resembles real‑world ambient noise than white noise\
-   Produces balanced spectral fingerprints for ML classification\
-   Helps models recognise unique frequency distributions of different
    environments

### Capture Process

The firmware records **30 clips**, each **2 seconds long**, of the
environment's natural soundscape.\
These clips later serve as ML training data, complementing T60
measurement with environmental categorisation.

------------------------------------------------------------------------

## 🛠️ Hardware Requirements

  ----------------------------------------------------------------------------
  Component         Description         Pinout
  ----------------- ------------------- --------------------------------------
  Microcontroller   ESP32‑S3            Core Processor
                    (WROOM‑1/N16R8)     

  Speaker/DAC       MAX98357A I2S       BCLK (1), LRC (2), DOUT (3)
                    Amplifier           

  Microphone        PDM MEMS Microphone PDM_CLK (42), PDM_DATA (41)

  Storage           MicroSD Card Module SPI (CS: 21, SCK: 7, MISO: 8, MOSI: 9)
  ----------------------------------------------------------------------------

------------------------------------------------------------------------

## 💻 Firmware (`ESP32S3_RIR_T60_...ino`)

The Arduino sketch controls:

-   I2S input (PDM mic)
-   I2S output (DAC amplifier)
-   SD card handling\
-   Active & passive recording workflows

### Key Firmware Features

-   **Dynamic Scaling:** A short test chirp determines optimal playback
    gain to avoid clipping while maximizing SNR.\
-   **RIR Capture:** Saves 5‑second WAV recordings named as
    `RIR_<location>_YYYYMMDD_HHMMSS.wav`.\
-   **Passive Capture:** Stores 30 × 2‑second ambient pink‑noise
    recordings for ML use.

------------------------------------------------------------------------

## 🐍 Python Post‑Processing (`batch_rir_processor.py`)

This script analyzes all WAV files from the SD card and computes
acoustic parameters.

### Dependencies

    pip install numpy matplotlib scipy pandas openpyxl

### Usage

1.  Place all `RIR_*.wav` files in the same directory as the script.\
2.  Run:

```{=html}
<!-- -->
```
    python batch_rir_processor.py

------------------------------------------------------------------------

## 📁 Output Artefacts

The script creates an **rir_analysis_output/** folder containing:

-   **RIR_T60_Summary.xlsx** --- Summary table of T20/T25/T30‑based T60
    values\
-   **Extracted RIR WAVs** --- Clean impulse responses\
-   **EDC CSV data** --- Time‑series decay curve samples\
-   **EDC Plot (PNG)** --- Schroeder integration + regression fit
    visualisation

------------------------------------------------------------------------

## 📘 Summary

This system enables:

-   Accurate room reverberation measurement using industry‑standard
    methodology\
-   Lightweight embedded data capture using an ESP32‑S3\
-   Automated post‑processing with exportable analytics\
-   Passive acoustic fingerprinting for ML‑driven environment
    classification
