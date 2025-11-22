# Acoustic Feature Analysis: Reverberation Time (T60) Measurement

## 🇬🇧 Project Overview

This repository contains the firmware and post-processing Python script
for a dedicated acoustic measurement system. The primary goal is to
accurately measure the Reverberation Time (T60) of an environment using
a low-cost ESP32-S3 microcontroller, following established acoustic
engineering principles.

The system performs two main types of data acquisition:

-   **Active Measurement:** Room Impulse Response (RIR) capture using a
    logarithmic sine sweep (chirp) for precise T60 calculation.
-   **Passive Measurement:** Ambient pink noise recording for
    environmental acoustic feature classification.

All raw audio data is recorded and stored on an SD card for batch
post-processing.

------------------------------------------------------------------------

## 🔬 Methodology for T60 Calculation

The T60 is calculated from the RIR using the Schroeder integration
method within the `batch_rir_processor.py` script.

### 1. RIR Acquisition (Sine Sweep)

The ESP32-S3 plays a logarithmic sine sweep (chirp) through the speaker
while simultaneously recording the signal via the MEMS microphone. The
recorded signal captures the room's influence (reflections, absorption)
on the sound.

### 2. RIR Extraction (Python)

The pure Room Impulse Response (RIR) is extracted by performing a linear
cross-correlation between the recorded room response and the known,
clean reference chirp.

### 3. Energy Decay Curve (EDC) and Extrapolation

The T60 is the time it takes for sound energy to decay by **60 dB**. The
post-processing script calculates the Energy Decay Curve (EDC) and
applies linear regression over specific decay segments to accurately
extrapolate the T60 value, even in the presence of a noise floor.

The script calculates three T60 variants for robust analysis:

-   **T20-based T60:** From −5 dB to −25 dB\
-   **T30-based T60:** From −5 dB to −35 dB\
-   **T25-based T60:** From −5 dB to −30 dB

------------------------------------------------------------------------

## 🛠️ Hardware Requirements

  ----------------------------------------------------------------------------
  Component         Description          Pinout
  ----------------- -------------------- -------------------------------------
  Microcontroller   ESP32-S3             Core Processor
                    (WROOM-1/N16R8)      

  Speaker/DAC       MAX98357A I2S        BCLK (1), LRC (2), DOUT (3)
                    Amplifier            

  Microphone        PDM MEMS Microphone  PDM_CLK (42), PDM_DATA (41)

  Storage           MicroSD Card Module  SPI Pins (CS: 21, SCK: 7, MISO: 8,
                                         MOSI: 9)
  ----------------------------------------------------------------------------

------------------------------------------------------------------------

## 💻 Firmware (`ESP32S3_RIR_T60_...ino`)

The Arduino sketch handles I2S initialisation for both output (DAC) and
input (PDM mic), SD card file management, and the sound
generation/capture sequence.

### Key Features

-   **Dynamic Scaling:** A test chirp determines optimal amplitude for
    best SNR without clipping.
-   **RIR Capture:** Saves 5-second raw audio to WAV (e.g.,
    `RIR_livingroom_YYYYMMDD_HHMMSS.wav`).
-   **Passive Capture:** Records thirty 2-second ambient pink-noise
    clips.

------------------------------------------------------------------------

## 🐍 Python Post-Processing (`batch_rir_processor.py`)

This script performs high-precision acoustic analysis on the WAV files
collected from the SD card.

### Dependencies

    pip install numpy matplotlib scipy pandas openpyxl

### Usage

1.  Copy all `RIR_*.wav` files into the script directory.
2.  Run:

```{=html}
<!-- -->
```
    python batch_rir_processor.py

------------------------------------------------------------------------

## 📁 Output Artefacts

The script creates an **rir_analysis_output/** directory containing:

-   **RIR_T60_Summary.xlsx:** Excel summary of T20, T25, T30 T60 values.
-   **Extracted RIR WAV files**
-   **CSV Decay Data:** Time-series EDC values.
-   **EDC Plot (PNG):** Schroeder integration with regression line.
