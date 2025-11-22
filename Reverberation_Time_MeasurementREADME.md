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


    T60 Measurement Protocol: Practical Feature Extraction vs. ISO Standard

Rationale for Measurement Methodology

The calculated Reverberation Time (T60) values in this dataset are employed as a crucial feature to classify the acoustic environment (e.g., "echoey" versus "damp") of each sampled room. My primary objective is to train a DL/TinyML model for robust environmental classification, not to generate certified acoustic reports.

Consequently, this project adopts a pragmatic, data-driven methodology that prioritises speed and internal consistency over strict compliance with professional acoustic standards like ISO 3382-1.

Why Single-Point Measurements Are Used

Feature Value is Relative: For my machine learning application, the model does not require the absolute, spatially-averaged T60 value. It only needs to learn the relative acoustic distinction between rooms. A T60 of $1.7\text{ s}$ is a strong indicator of a reflective space compared to a T60 of $0.5\text{ s}$ for an absorptive space. This differentiation remains robust even with single-point measurements.

Capturing Localised Acoustic Reality: High measured values (e.g., the $\approx 1.7\text{ s}$ observed in the conservatory) are accepted as accurate feature representations. These values often capture the effect of strong localised Room Modes (standing waves), which are a genuine acoustic characteristic of that specific location and are highly relevant to the resulting pink noise data recorded there.

Efficiency for Scale: Adhering to the ISO standard's multi-point measurement requirement (4-5 locations per room) is computationally and time-intensive. For rapid, large-scale data collection, a single, highly consistent measurement point per room provides the optimum trade-off between feature fidelity and acquisition speed.

T60 as a Relative Gauge (High Values Accepted)

In professional acoustics, multiple measurements are averaged to obtain a precise, room-wide T60 suitable for building regulations or compliance. For this project, however, the calculated T60 is treated simply as a high-fidelity gauge for relative "echoeyness." I accept that single-point measurements, especially in highly reflective rooms like the Conservatory, will yield higher, locally focused values (e.g., $1.7\text{ s}$) that may differ from a true, spatially averaged measurement. Crucially, this high value still correctly and robustly signals the room's highly reflective nature compared to a well-damped room's $\approx 0.5\text{ s}$ reading. Since the pink noise data is collected at this exact same location, the high T60 acts as a perfect correlating feature, linking the acoustic character of the room at the point of data capture to the raw audio features processed by the DL model. 

Standardised Procedure (Mandatory for Internal Consistency)

To ensure that the T60 values remain comparable across all collected data (which is paramount for ML training), the following procedure is strictly maintained:

Fixed Rig Placement: Both the loudspeaker (source) and the microphone (receiver) must be placed on rigid, non-absorptive supports (stands or hard tables).

Consistent Height: A consistent height (e.g., $1.2\text{ m}$) for the microphone is used across all rooms.

Operator/Furniture Exclusion: Measurements are never taken directly on soft, absorptive materials (like sofas or beds) or near the operator, as this introduces inconsistent high-frequency absorption that skews the relative feature values.
