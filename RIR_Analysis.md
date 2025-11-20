Room Impulse Response (RIR) and Reverberation Time Analysis

This repository contains the data and theoretical background for analyzing the acoustic characteristics of a space using its Room Impulse Response (RIR). The primary goal is to determine the Reverberation Time ($\text{T}_{60}$), a key metric in architectural acoustics, and compare different domestic environments.

1. Theoretical Background

1.1. Room Impulse Response (RIR)

The Room Impulse Response (RIR), denoted as $h(t)$ in acoustic literature, is the acoustic fingerprint of a space. It describes how sound waves travel from a source (e.g., a speaker) to a receiver (e.g., a microphone) within that room.

The RIR is theoretically the sound recorded when an instantaneous, ideal impulse of sound is generated. In practice, due to the difficulty of generating an ideal impulse, it is typically measured by playing a known test signal (like an Exponential Sine Sweep) and mathematically de-convolving the recorded signal to derive the RIR.

1.2. Reverberation Time (T60)

Reverberation Time ($\text{T}_{60}$ or T60) is the most critical metric extracted from the RIR. It is defined as the time required for the sound energy in a room to decay by 60 decibels (dB) after the sound source has stopped.

A short $\text{T}_{60}$ implies an acoustically "dry" room (good for speech clarity). A long $\text{T}_{60}$ implies an acoustically "live" or "reverberant" room (like a cathedral).

1.3. Schroeder Integration and Energy Decay Curve (EDC)

The Schroeder Reverse Integration Method is used to derive a smooth decay curve, known as the Energy Decay Curve (EDC), from the raw RIR. This mathematical approach minimizes the effects of noise.

The mathematical concept for the reverse integration is:

$$E(t) = \int_{t}^{\infty} h^2(\tau) d\tau$$

Note: In the acoustic analysis script, this is calculated via discrete summation of the squared RIR data points in reverse.

2. Analysis Methods for Noise-Limited Data

Accurate $\text{T}_{60}$ calculation requires a 60 dB Dynamic Range (the difference between the peak signal and the ambient noise floor). When the dynamic range is less than 60 dB, we use partial decay measures and extrapolate:

T20: The time taken for the sound energy to decay by 20 dB (from $-5$ dB to $-25$ dB).

Extrapolation Formula: T60 = 3 * T20

T30: The time taken for the sound energy to decay by 30 dB (from $-5$ dB to $-35$ dB).

Extrapolation Formula: T60 = 2 * T30

3. Comparative Case Studies: Living Room vs. Conservatory

This section compares the acoustic characteristics of two distinct residential spaces measured using the RIR method.

3.1. Case Study 1: Carpeted Living Room

This analysis is based on the pre-processed data in rir_energy_decay_data.csv.

Parameter

Value

Deduction

Room Type

Residential Living Room

Expected T60: Short

Acoustic Treatment

Fully carpeted floor, sofa, soft furnishings (absorptive)

High absorption leads to rapid sound decay.

Dynamic Range

$\approx -29$ dB (Noise Floor)

Measurement is noise-limited; requires T20/T30 extrapolation.

Estimated T60

0.4 s to 0.7 s

Excellent for speech clarity and comfortable listening. The room is acoustically "dry."

3.2. Case Study 2: Conservatory

This analysis is based on the raw and extracted RIR files (rir.wav and extracted_rir.wav).

Physical Feature

Acoustic Effect

Expected T60

Construction

Large glass walls and hard flooring (reflective)

High reflection leads to slow sound decay.

Expected Behavior

Significant echo and potential flutter echo between parallel surfaces.

Sound energy is largely trapped.

Estimated T60

1.0 s or higher

Poor speech intelligibility; the room is acoustically "live" or "reverberant."

3.3. Next Steps in Analysis

To quantify the difference between the two spaces, the next step is to process the Conservatory data:

Load the RIR from the provided WAV files (rir.wav or extracted_rir.wav).

Calculate the Energy Decay Curve (EDC) using the Schroeder Reverse Integration Method.

Perform linear regression on the linear decay segment of the Conservatory EDC.

Calculate and output the precise T60 value for the Conservatory for direct comparison with the Living Room's results.

This document outlines the theoretical foundation and contextualizes the ongoing acoustic analysis project.
