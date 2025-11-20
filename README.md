# 🔊 TinyML Acoustic Feature Recognition System (AFRS)

### Passive Classification of Room Acoustics using ESP32

This project describes a complete system for **Acoustic Feature Recognition (AFR)**, leveraging an ESP32-S3 microcontroller and a machine learning pipeline (TinyML) to **passively classify a room's acoustic quality** (liveliness/dryness) by analyzing background sounds.

The system uses an **Active Measurement (RIR)** phase solely for initial **Ground-Truth Labeling** and then deploys a small neural network to perform **Passive Inference** in real-time.

---

## 1. 🔬 Methodology: Active Calibration to Passive Inference

The project is structured into two distinct data collection and processing phases:

### Phase A: Active RIR Measurement (Ground-Truth Labeling)

This process uses a known stimulus (a chirp) and measurement to establish an objective, scientific label for a room's acoustics.

| Metric | Role | Derived From |
| :--- | :--- | :--- |
| **Room Impulse Response (RIR)** | The room's acoustic fingerprint. | Active chirp playback and simultaneous recording. |
| **Reverberation Time ($T_{60}$)** | The definitive measure of "liveliness." | Calculated from the **Energy Decay Curve (EDC)** derived from the RIR. |

### Phase B: Passive Data Logging (Training Data)

Once the $T_{60}$ is known, the system is switched to silent, passive mode to collect the actual training data. All recorded background audio clips are labeled with the $T_{60}$-derived category (e.g., "Lively," "Dry").

---

## 2. 💾 Data Acquisition & Hardware Configuration

The system relies on a high-fidelity ESP32-S3 setup utilizing its dual-core architecture and I2S peripherals.

### 2.1 Active Capture Sketch (`Acoustic_Feature_Recognition_Capture.ino`)

The initial sketch is designed for precision RIR measurement:

* **Technique:** Logarithmic sweep (chirp) broadcasted via I2S TX and recorded via I2S PDM RX simultaneously.
* **Synchronization:** FreeRTOS binary semaphores are used to guarantee the microphone recording begins precisely before the chirp starts playing.
* **Initial Bug Fix:** The original sketch was corrected to remove a severe **digital clipping** issue caused by an excessive post-recording `VOLUME_GAIN` of `16.0`.

| Component | Function | I2S Port | CPU Core |
| :--- | :--- | :--- | :--- |
| **Playback Task** | Generates and outputs Chirp. | I2S Port 1 (TX) | Core 1 (App CPU) |
| **Recording Task** | Captures PDM microphone data. | I2S Port 0 (RX) | Core 0 (Pro CPU) |

### 2.2 Passive Logging Sketch (`TinyML_Audio_Logger.ino`)

This simplified sketch is used after calibration to collect hundreds of 1-second background noise clips for training. It removes all playback and chirp generation logic.

---

## 3. 🧠 TinyML Model Training Pipeline

The collected data is processed to train a low-memory model suitable for the ESP32-S3.

### 3.1 Feature Engineering for Acoustic Classification

The model is trained on **passive background audio clips**, which requires features sensitive to the subtle temporal smear that reverberation causes in ambient noise:

* **Target Labels:** Categories derived from the RT60 calculated in Phase A (e.g., "RT60 < 0.4s (Dry)", "RT60 > 0.8s (Lively)").
* **Extracted Features:**
    * **Temporal Decay:** Zero-Crossing Rate (ZCR), RMS Energy decay.
    * **Spectral Smearing:** Spectral Centroid, Spectral Flatness, and Mel-Frequency Cepstral Coefficients (MFCCs).

### 3.2 Model Optimization

* **Model Type:** Small CNN or RNN suitable for sequence classification.
* **Quantization:** Weights are reduced from 32-bit float to 8-bit integers, which is essential for fitting the model into the ESP32's PSRAM and enabling fast inference.

---

## 4. 🚀 Deployment and Passive Inference

The optimized TFLite model is integrated directly into the ESP32 firmware using **TensorFlow Lite Micro (TFLM)**.

### 4.1 On-Device Passive Inference Cycle

The ESP32 runs a continuous, ultra-low-power cycle:

1.  **Record:** Capture a short segment of ambient audio.
2.  **Feature Extraction:** Calculate the required ZCR and Spectral features in real-time.
3.  **Inference:** Features flow through the TFLite Micro model.
4.  **Prediction:** The model outputs the room classification (e.g., "Acoustic Quality: Dry").

### 4.2 Application and HCI Relevance

This project shifts acoustic sensing from simple monitoring (keyword spotting) to **active environmental context awareness**, enabling intelligent control systems.

| Contextual Awareness | HCI Relevance |
| :--- | :--- |
| Classifies **physical acoustic properties** (e.g., large reflective room vs. small damp office). | **Adaptive Audio Interfaces:** Voice assistants can adjust microphone gain, noise reduction, or EQ based on the classified room type. |
| **Change Detection:** Detects major acoustic shifts (e.g., furniture added/removed). | **Smart Building Control:** HVAC or lighting can adapt to the classified room type for optimal comfort and energy use. |
