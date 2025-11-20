import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy.signal import chirp, convolve
import csv

# --- Configuration matching the Arduino sketch ---
SAMPLE_RATE = 16000
RECORD_SECONDS = 5
F_START = 500.0
F_END = 4000.0
INPUT_WAV_FILE = "rir.wav"
OUTPUT_RIR_FILE = "extracted_rir.wav"
OUTPUT_CSV_FILE = "rir_energy_decay_data.csv" # New output file

# --- Functions from previous script (omitted for brevity) ---
def generate_chirp_signal(rate, duration, f0, f1):
    """Generates the reference logarithmic sine sweep (chirp)."""
    t = np.linspace(0, duration, int(rate * duration), endpoint=False)
    reference_chirp = chirp(t, f0=f0, f1=f1, t1=duration, method='logarithmic', phi=0)
    reference_chirp_scaled = reference_chirp * 32767.0
    return reference_chirp_scaled.astype(np.int16)

def extract_rir(recorded_sweep, reference_chirp):
    """Performs the deconvolution using the inverse filter method (cross-correlation)."""
    time_reversed_chirp = reference_chirp[::-1]
    rir_raw = convolve(recorded_sweep, time_reversed_chirp, mode='same')
    
    rir_max = np.max(np.abs(rir_raw))
    if rir_max > 0:
        rir_normalized = rir_raw / rir_max
    else:
        rir_normalized = rir_raw
        
    return (rir_normalized * 32767).astype(np.int16)

# --- NEW Function for Acoustic Analysis Output ---
def save_energy_decay_data(rir_data, filename, rate):
    """
    Calculates the Schroeder-style Energy Decay Curve (EDC) and saves it to a CSV file.
    The EDC is derived by reverse-time integration of the squared impulse response.
    """
    # 1. Square the RIR (Energy)
    rir_squared = rir_data.astype(np.float64)**2

    # 2. Reverse-Time Integration (Schroeder Integration)
    # The integration is done from the end of the RIR back to the beginning.
    energy_decay = np.flip(np.cumsum(np.flip(rir_squared)))
    
    # 3. Convert to Logarithmic Scale (dB)
    # Reference for 0 dB is the maximum energy level (at time t=0 of the decay).
    energy_max = energy_decay[0]
    if energy_max == 0:
        print("WARNING: Max energy is zero. Cannot calculate log scale decay.")
        return
        
    # Scale to dB: 10 * log10(E(t) / E_max)
    energy_decay_db = 10 * np.log10(energy_decay / energy_max)
    
    # 4. Prepare data for CSV
    time_vector = np.linspace(0, len(rir_data) / rate, len(rir_data), endpoint=False)
    
    # Write to CSV
    with open(filename, 'w', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        # Write header
        csv_writer.writerow(['Time (s)', 'Energy Decay (dB)'])
        # Write data rows
        for t, db in zip(time_vector, energy_decay_db):
            csv_writer.writerow([t, db])
            
    print(f"Energy decay data saved to '{filename}'.")

# --- Modified Main Execution ---
def visualize_rir(rir_data):
    """Plots the extracted RIR in the time domain."""
    t = np.linspace(0, len(rir_data) / SAMPLE_RATE, len(rir_data), endpoint=False)
    
    plt.figure(figsize=(12, 6))
    
    plt.subplot(2, 1, 1)
    plt.plot(t, rir_data)
    plt.title('Extracted Room Impulse Response (RIR) - Time Domain')
    plt.xlabel('Time (s)')
    plt.ylabel('Amplitude')
    plt.grid(True)
    
    zoom_samples = int(0.1 * SAMPLE_RATE)
    plt.subplot(2, 1, 2)
    plt.plot(t[:zoom_samples], rir_data[:zoom_samples])
    plt.title('RIR - Zoomed on First 100ms')
    plt.xlabel('Time (s)')
    plt.ylabel('Amplitude')
    plt.grid(True)
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    print(f"--- RIR Extraction & Analysis Output Script ---")

    # 1. Load the recorded sweep data
    try:
        rate, recorded_data = wavfile.read(INPUT_WAV_FILE)
    except FileNotFoundError:
        print(f"ERROR: Input file '{INPUT_WAV_FILE}' not found. Please ensure it's in the same directory.")
        exit()
    except Exception as e:
        print(f"ERROR reading WAV file: {e}")
        exit()

    if rate != SAMPLE_RATE:
        print(f"WARNING: WAV file rate ({rate} Hz) does not match script rate ({SAMPLE_RATE} Hz). Using WAV file rate.")
        SAMPLE_RATE = rate
        
    if recorded_data.ndim > 1:
        recorded_data = recorded_data[:, 0]

    print(f"Loaded recorded sweep with {len(recorded_data)} samples at {rate} Hz.")
    
    # 2. Generate the reference chirp
    ref_chirp = generate_chirp_signal(SAMPLE_RATE, RECORD_SECONDS, F_START, F_END)
    
    min_len = min(len(recorded_data), len(ref_chirp))
    recorded_data = recorded_data[:min_len]
    ref_chirp = ref_chirp[:min_len]

    # 3. Extract the RIR
    print("Performing deconvolution (cross-correlation)...")
    extracted_rir = extract_rir(recorded_data, ref_chirp)
    print("Deconvolution complete.")

    # 4. Save the final RIR as an audio file
    wavfile.write(OUTPUT_RIR_FILE, SAMPLE_RATE, extracted_rir)
    print(f"Extracted RIR saved to '{OUTPUT_RIR_FILE}'.")
    
    # 5. Save the Energy Decay Curve data to CSV (for T60 analysis)
    save_energy_decay_data(extracted_rir, OUTPUT_CSV_FILE, SAMPLE_RATE)

    # 6. Visualize the RIR (Time Domain)
    visualize_rir(extracted_rir)