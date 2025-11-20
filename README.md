🔊 TinyML Acoustic Feature Recognition System (AFRS)

This project describes a complete end-to-end system for Acoustic Feature Recognition, leveraging an ESP32 microcontroller, dual-channel I2S for high-fidelity audio capture, a machine learning pipeline (TinyML), and deployment for real-time human-computer interaction (HCI) applications.

The core goal is to enable an edge device to autonomously characterize its acoustic environment—such as room size, occupancy, or material state—by analyzing its Room Impulse Response (RIR).

1. 🎤 Data Acquisition & Synchronization (ESP32)

This initial phase focuses on capturing the high-quality acoustic data (the RIR) required to train the machine learning model. This is achieved using a tightly synchronized, dual-I2S, dual-core architecture on the ESP32 for high fidelity and zero-latency measurement.

1.1 The RIR Measurement Technique

The system measures the RIR by broadcasting a known logarithmic sweep (chirp) signal and simultaneously recording the echoes using a PDM microphone. This precise synchronization prevents artifacts and is essential for accurate feature extraction.

Stimulus: 20 Hz to 20 kHz Logarithmic Chirp (1 second duration).

Capture: 16-bit PCM, 44.1 kHz sample rate.

Result: A raw audio file containing the initial chirp followed by its reflections and reverberation.

1.2 Dual-Core Architecture and Synchronization

To ensure deterministic timing and maximum processing bandwidth, the tasks are split across the ESP32's two cores using FreeRTOS. The synchronization mechanism (semaphore) guarantees that recording begins precisely before the chirp signal starts playing.

Component	Function	I2S Port	CPU Core	Role
Playback Task	Generates and outputs Chirp via I2S.	I2S Port 1 (TX)	Core 1 (App CPU)	Stimulus generation
Recording Task	Captures PDM microphone data and saves to SD card.	I2S Port 0 (RX)	Core 0 (Pro CPU)	Data capture & storage
Synchronization	FreeRTOS Binary Semaphore	N/A	Both	Ensures recording starts before playback
2. 🧠 TinyML Model Training Pipeline

Once raw RIR files are collected from various environmental conditions (e.g., small room, large hall, empty vs. occupied, hard vs. soft surfaces), they are processed to train a highly efficient deep learning model.

2.1 Pre-processing and Feature Engineering

Deconvolution: The raw RIR signal is mathematically deconvolved with the original chirp signal to isolate the true Impulse Response.

Acoustic Feature Extraction: The RIR is analyzed to extract standard acoustic features:

𝑇
60
T
60
	​

 (Reverberation Time)

Clarity (
𝐶
50
,
𝐶
80
C
50
	​

,C
80
	​

)

Definition (
𝐷
50
D
50
	​

)

Spectral features (e.g., MFCCs) of the impulse response envelope

Labeling: Features are labeled based on the ground truth of the environment (e.g., large_conference_room, occupied).

2.2 Model Selection and Optimization

Model Type: Small CNN or RNN suitable for sequence classification.

Training: Performed in Python using TensorFlow/Keras, then converted to TFLite.

Quantization: Weights reduced from 32-bit float to 8-bit integers—critical for fitting inside ESP32 PSRAM and enabling fast inference.

3. 🚀 Deployment and Inference

The optimized TFLite model is integrated directly into the ESP32 firmware using TensorFlow Lite Micro, enabling real-time environmental characterization at the edge.

3.1 On-Device Inference Cycle

The ESP32 runs a full acoustic recognition cycle:

Playback

Record

Deconvolution

Feature Extraction

Extracted features flow through the TFLite Micro model, which outputs a prediction such as “Medium Room, Unoccupied”.

Power Management: Device can deep-sleep between measurements for ultra-low-power monitoring.

3.2 Hardware Requirements Summary

Microcontroller: ESP32/ESP32-S3 (PSRAM required)

Input: High-quality PDM microphone

Output: I2S DAC/Amplifier (e.g., MAX98357A) + speaker

Storage: SD Card

4. 💡 Application and HCI Relevance

This project shifts acoustic sensing from passive monitoring (e.g., keyword spotting) to active environmental characterization, opening new possibilities for Human-Computer Interaction.

(Image reference: Getty Images)

What it Does (Contextual Awareness)

The system classifies physical acoustic properties of the environment:

Room State: large reflective room vs. small damp office

Change Detection: furniture rearranged, partitions added

Occupancy Proxy: reverberation-time changes with people in the space

Why it is Useful in HCI

Adaptive Audio Interfaces: Voice assistants adjust EQ, speed, etc.

Smart Building Control: HVAC/lighting adapts to classified room type

Implicit Input: Acoustic changes as triggers (e.g., door slam → event)

Energy Efficiency: Fine-grained, environment-aware power decisions
