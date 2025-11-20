Room Impulse Response (RIR) and Reverberation Time Analysis

This repository contains the data and theoretical background for analyzing the acoustic characteristics of a space using its Room Impulse Response (RIR). The primary goal is to determine the Reverberation Time ($\text{T}_{60}$), a key metric in architectural acoustics.

1. Theoretical Background

1.1. Room Impulse Response (RIR)

The Room Impulse Response (RIR), denoted as $h(t)$, is the acoustic fingerprint of a space. It describes how sound waves travel from a source (e.g., a speaker) to a receiver (e.g., a microphone) within that room.

The RIR is theoretically the sound recorded when an instantaneous, ideal impulse of sound is generated. In practice, due to the difficulty of generating an ideal impulse, it is typically measured by playing a known test signal (like an Exponential Sine Sweep, as used in this measurement) and mathematically de-convolving the recorded signal to derive the RIR.

1.2. Reverberation Time ($\text{T}_{60}$)

Reverberation Time ($\text{T}_{60}$) is the most critical metric extracted from the RIR. It is defined as the time required for the sound energy in a room to decay by 60 decibels (dB) after the sound source has stopped.

A short $\text{T}_{60}$ implies an acoustically "dry" room (good for speech clarity, like a classroom). A long $\text{T}_{60}$ implies an acoustically "live" or "reverberant" room (like a cathedral or concert hall).

1.3. Schroeder Integration and Energy Decay Curve (EDC)

The rir_energy_decay_data.csv file is generated using the Schroeder Reverse Integration Method.

This method involves integrating the squared RIR signal backwards in time to create a smooth, cumulative decay curve, known as the Energy Decay Curve (EDC). This mathematical approach minimizes the effects of noise and non-linearities in the measured RIR, providing a much cleaner decay plot than integrating forward.

The mathematical formula for the reverse integration is:

$$E(t) = \int_{t}^{\infty} h^2(\tau) d\tau
$$Where:

* $E(t)$ is the energy decay at time $t$.
* $h^2(\tau)$ is the squared impulse response.
* The result is normalized so the maximum value (at $t=0$) is $0$ dB.

## 2\. Analysis Methods for Noise-Limited Data

Accurate $\text{T}_{60}$ calculation requires a **60 dB Dynamic Range** (the difference between the peak signal and the ambient noise floor).

When the dynamic range is less than 60 dB, we use partial decay measures and extrapolate:

1.  **$\text{T}_{20}$:** The time taken for the sound energy to decay by 20 dB (from $-5$ dB to $-25$ dB).$$

$$\\text{T}*{60} = 3 \\times \\text{T}*{20}

$$
$$


$\text{T}_{30}$: The time taken for the sound energy to decay by 30 dB (from $-5$ dB to $-35$ dB).

$$\\ \text{T}*{60} = 2 \times \text{T}*{30}$$

$$$$

3. Case Study: Living Room Measurement

3.1. Context and Setup

Parameter

Value

Room Type

Residential Living Room

Acoustic Treatment

Fully carpeted floor, sofa, soft furnishings (absorptive)

Measurement Signal

Exponential Sine Sweep (ESS)

Recording Duration

5 seconds (as seen in the rir_energy_decay_data.csv)

Data Source

rir_energy_decay_data.csv

3.2. Deductions from the Energy Decay Curve (EDC)

The analysis of the EDC data provided the following key insights:

Low Dynamic Range: The decay curve flattens out significantly after approximately $0.5$ seconds, reaching a noise floor of roughly $-29$ dB.

Interpretation: The measurement is noise-limited. The ambient noise (from internal or external sources) is only 29 dB below the peak signal, preventing the measurement of the full 60 dB decay. This necessitates the use of $\text{T}_{20}$ or $\text{T}_{30}$ extrapolation.

Acoustically Dry Environment: The initial, valid portion of the decay is very steep.

Interpretation: This rapid decay confirms the acoustic impact of the carpeted floors and soft furnishings. These materials are highly sound-absorbent, leading to a short reverberation time.

Estimated $\text{T}_{60}$: Based on typical residential room acoustics and the steepness of the initial decay, the true $\text{T}_{60}$ is likely in the $0.4 \text{s}$ to $0.7 \text{s}$ range. This is excellent for speech intelligibility and typical for a comfortable domestic space.

3.3. Next Steps in Analysis

To calculate the precise $\text{T}_{60}$ value:

Load the rir_energy_decay_data.csv.

Identify the linear decay region (e.g., from $-5$ dB to the noise floor at $-25$ dB).

Perform a linear regression on this segment to determine the slope ($m$).

Calculate $\text{T}_{20}$ (the time over which the 20 dB decay occurs).

Extrapolate using the formula: $\text{T}_{60} = 3 \times \text{T}_{20}$.

This document serves as the foundation for the acoustic analysis script, which will perform the final linear fitting and $\text{T}_{60}$ calculation.
