# ==============================================================================
# ACOUSTIC ISOLATION AND T60 RIR ANALYSIS SCRIPT
# ==============================================================================
#
# CONTEXT:
# The user's T60 measurements (broadband 1.677s, 1kHz filtered 2.220s) are
# significantly inflated above the expected 0.5s - 1.2s range for a living room.
# This inflation is attributed to a strong, early acoustic reflection off the
# nearby laptop's screen and body, which is required for serial communication.
#
# GOAL:
# 1. Prompt the user to implement "Acoustic Cloaking" (shielding the laptop).
# 2. Instruct the user to capture a new, clean RIR sweep using the ESP32.
# 3. Process the new RIR file using the stable, broadband T20/T30 calculation
#    to find the true, stable T60 of the room, free from the laptop reflection.
#
# The T20/T30 fit method is robust and preferred over the unstable 1kHz filtering.
# ==============================================================================

import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy.signal import chirp
from scipy.fft import fft, ifft, fftfreq 
import csv
import glob
import os
import pandas as pd 
import time

# --- Configuration matching the Arduino sketch ---
SAMPLE_RATE = 16000
RECORD_SECONDS = 5
F_START = 500.0                 
F_END = 4000.0                  
INPUT_WAV_PATTERN = "RIR_*.wav"     
OUTPUT_DIR = "rir_analysis_output"
OUTPUT_EXCEL_FILE = "RIR_T60_Summary_Final.xlsx" 

# --- Fit Ranges for T20, T25, T30 (Broadband Analysis) ---
FIT_RANGES = {
    'T20': (-5.0, -25.0),  # T20 fit: 20 dB decay (T20 * 3 = T60)
    'T30': (-5.0, -35.0),  # T30 fit: 30 dB decay (T30 * 2 = T60)
    'T25': (-5.0, -30.0)   # Previous fit for comparison
}

# --- Helper Functions ---
def generate_chirp_signal(rate, duration, f0, f1):
    """Generates the reference logarithmic sine sweep (chirp)."""
    t = np.linspace(0, duration, int(rate * duration), endpoint=False)
    reference_chirp = chirp(t, f0=f0, f1=f1, t1=duration, method='logarithmic', phi=0)
    reference_chirp_scaled = reference_chirp * 32767.0
    return reference_chirp_scaled.astype(np.int16)

def extract_rir(recorded_sweep, reference_chirp):
    """Deconvolution using FFT with a frequency mask."""
    recorded_sweep_float = recorded_sweep.astype(np.float64)
    reference_chirp_float = reference_chirp.astype(np.float64)
    N = len(recorded_sweep_float)
    
    Y = fft(recorded_sweep_float) 
    X = fft(reference_chirp_float) 
    
    epsilon = 1e-12 
    H_inv = np.conj(X) / (np.abs(X)**2 + epsilon)
    
    freqs = fftfreq(N, 1/SAMPLE_RATE)
    mask = (np.abs(freqs) >= F_START) & (np.abs(freqs) <= F_END)
    H_inv_filtered = H_inv * mask 
    
    H_rir_fft = Y * H_inv_filtered
    rir_raw = np.real(ifft(H_rir_fft))
    
    rir_max = np.max(np.abs(rir_raw))
    if rir_max > 0:
        rir_normalized = rir_raw / rir_max
    else:
        rir_normalized = rir_raw
        
    return (rir_normalized * 32767).astype(np.int16)

def calculate_energy_decay(rir_data, rate):
    """Calculates the Schroeder-style Energy Decay Curve (EDC) in dB."""
    rir_squared = rir_data.astype(np.float64)**2
    energy_decay = np.flip(np.cumsum(np.flip(rir_squared)))
    
    energy_max = energy_decay[0]
    if energy_max == 0:
        return None, None
        
    energy_decay_db = 10 * np.log10(energy_decay / energy_max)
    time_vector = np.linspace(0, len(rir_data) / rate, len(rir_data), endpoint=False)
    
    return time_vector, energy_decay_db

def calculate_t_metrics(time_vector, energy_decay_db):
    """Calculates T60 extrapolations based on T20, T30, and T25 fits."""
    results = {}

    for metric, (start_db, end_db) in FIT_RANGES.items():
        valid_indices = np.where((energy_decay_db >= end_db) & (energy_decay_db <= start_db))

        if len(valid_indices[0]) < 2:
            results[f'{metric}_T60'] = np.nan
            results[f'{metric}_Slope'] = np.nan
            results[f'{metric}_Intercept'] = np.nan
            continue

        decay_times = time_vector[valid_indices]
        decay_levels = energy_decay_db[valid_indices]
        
        slope, intercept = np.polyfit(decay_times, decay_levels, 1)

        if slope >= 0:
            results[f'{metric}_T60'] = np.nan
            results[f'{metric}_Slope'] = np.nan
            results[f'{metric}_Intercept'] = np.nan
            continue

        t60_estimate = 60.0 / np.abs(slope)
        
        results[f'{metric}_T60'] = t60_estimate
        results[f'{metric}_Slope'] = slope
        results[f'{metric}_Intercept'] = intercept
        
    return results

def visualize_decay(time_vector, energy_decay_db, room_name, metrics_results, fit_metric='T30', plot_save_path=None):
    """Plots the EDC with the specified linear fit (defaulting to T30)."""
    
    start_db, end_db = FIT_RANGES.get(fit_metric, (-5.0, -35.0))
    t60 = metrics_results.get(f'{fit_metric}_T60', np.nan)
    slope = metrics_results.get(f'{fit_metric}_Slope', np.nan)
    intercept = metrics_results.get(f'{fit_metric}_Intercept', 0)
    
    if np.isnan(t60):
        if fit_metric == 'T30' and not np.isnan(metrics_results.get('T20_T60')):
            return visualize_decay(time_vector, energy_decay_db, room_name, metrics_results, 'T20', plot_save_path)
        print(f"Cannot plot fit for {fit_metric} as data is insufficient.")
        return

    plt.figure(figsize=(10, 5))
    
    plt.plot(time_vector, energy_decay_db, label='Schroeder Energy Decay Curve (EDC)', color='blue')
    
    fit_line = slope * time_vector + intercept
    plt.plot(time_vector, fit_line, 'r--', linewidth=2, 
             label=f'T{abs(start_db - end_db):.0f} Linear Fit (Slope: {slope:.2f} dB/s)')
        
    plt.axvline(x=0, color='k', linestyle='--', linewidth=0.5)
    plt.axhline(y=start_db, color='green', linestyle=':', label=f'Fit Start ({start_db:.1f} dB)')
    plt.axhline(y=end_db, color='green', linestyle=':', label=f'Fit End ({end_db:.1f} dB)')
        
    plt.title(f'Energy Decay Curve and T60 Estimation for {room_name}\nEstimated T60: {t60:.3f} s (via {fit_metric} fit)')
    plt.xlabel('Time (s)')
    plt.ylabel('Energy Decay (dB)')
    plt.ylim(-70, 5) 
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.legend()
    
    if plot_save_path:
        plt.savefig(plot_save_path)
        plt.close()
        print(f"Decay plot (showing {fit_metric} fit) saved to '{plot_save_path}'.")
    else:
        plt.show()

def process_rir_file(input_filepath, ref_chirp):
    """Handles the full RIR extraction and analysis pipeline for a single file."""
    print(f"\n==============================================")
    print(f"Processing file: {os.path.basename(input_filepath)}")
    global SAMPLE_RATE
    
    try:
        rate, recorded_data = wavfile.read(input_filepath)
    except Exception as e:
        print(f"ERROR reading WAV file '{input_filepath}': {e}")
        return None
        
    min_len = min(len(recorded_data), len(ref_chirp))
    recorded_data = recorded_data[:min_len]
    ref_chirp = ref_chirp[:min_len]

    basename = os.path.splitext(os.path.basename(input_filepath))[0]
    rir_filename = os.path.join(OUTPUT_DIR, f"{basename}_extracted_RIR.wav")
    csv_filename = os.path.join(OUTPUT_DIR, f"{basename}_rir_energy_decay_data.csv")
    plot_filename = os.path.join(OUTPUT_DIR, f"{basename}_EDC.png")
    
    print("-> Performing FORTIFIED FFT-based deconvolution (Broadband RIR)...")
    extracted_rir = extract_rir(recorded_data, ref_chirp)
    wavfile.write(rir_filename, SAMPLE_RATE, extracted_rir)
    
    time_vector, energy_decay_db = calculate_energy_decay(extracted_rir, SAMPLE_RATE)
    if time_vector is None:
        return None
    
    metrics_results = calculate_t_metrics(time_vector, energy_decay_db)
    
    plot_metric = 'T30'
    if np.isnan(metrics_results.get('T30_T60', np.nan)):
        plot_metric = 'T20'
        
    if not np.isnan(metrics_results.get(f'{plot_metric}_T60', np.nan)):
        visualize_decay(time_vector, energy_decay_db, basename, metrics_results, plot_metric, plot_filename)
        
    # **PRINT ALL T60 AND SLOPE TO TERMINAL**
    print(f"\n*** RIR ANALYSIS RESULTS (Broadband T20/T30) ***")
    
    result_output = {'Filename': os.path.basename(input_filepath)}
    
    for metric in ['T20', 'T30', 'T25']:
        t60 = metrics_results.get(f'{metric}_T60', np.nan)
        slope = metrics_results.get(f'{metric}_Slope', np.nan)
        result_output.update({f'{metric}_T60 (s)': t60})

        if not np.isnan(t60):
            start_db, end_db = FIT_RANGES[metric]
            print(f"| {metric} Fit (-{abs(end_db):.0f}dB): T60={t60:.3f} s, Slope={slope:.3f} dB/s")
        else:
             print(f"| {metric} Fit: FAILED (Insufficient clean decay)")

    print(f"***************************\n")
    
    with open(csv_filename, 'w', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        csv_writer.writerow(['Time (s)', 'Energy Decay (dB)'])
        for t, db in zip(time_vector, energy_decay_db):
            csv_writer.writerow([t, db])
    
    return result_output

# --- Setup and Prompt Function ---
def setup_and_prompt():
    """Prompts the user for physical setup and finds the newest RIR file."""
    
    print("\n\n========================================================")
    print("       🔥 FINAL RIR MEASUREMENT STRATEGY: ACOUSTIC CLOAKING")
    print("========================================================\n")
    
    print("Based on our discussion, the inflated T60 (2.220s @ 1kHz) is a known\nproblem caused by the laptop's hard surfaces creating a strong, short-delay\nreflection that corrupts the RIR.")
    print("\n*** ACTION REQUIRED: Physical Setup ***")
    print("You must eliminate this reflection using materials you have on hand.")
    print("---------------------------------------")
    
    print("\nSTEP 1: POSITION THE RIG")
    print("   -> Ensure the ESP32 speaker/mic rig is pointing AWAY from the laptop.")
    
    print("\nSTEP 2: BUILD THE BARRIER (Acoustic Cloaking)")
    print("   -> Use THICK, soft materials (folded blankets, duvets, pillows) to build")
    print("      a substantial wall between the ESP32 rig and the laptop.")
    print("   -> The barrier must be taller than the laptop screen.")
    print("   -> This isolates the laptop while still allowing USB-C connection.")
    print("")
    
    print("\nSTEP 3: CAPTURE NEW DATA")
    input("\nPRESS ENTER after the physical setup is complete and you are ready to run the new RIR sweep on your ESP32...")
    
    print("\n---------------------------------------")
    print("Now, trigger the RIR sweep on your ESP32 via the serial monitor.")
    print("Wait for the sweep to complete and the new WAV file to save on the SD card.")
    print("---------------------------------------")
    
    input("\nPRESS ENTER once the new RIR file is saved and ready for analysis...")
    
    return find_newest_rir_file()

def find_newest_rir_file():
    """Finds the most recently modified RIR file in the current directory."""
    rir_files = glob.glob(INPUT_WAV_PATTERN)
    if not rir_files:
        print(f"\nERROR: No files matching '{INPUT_WAV_PATTERN}' found.")
        return None
    
    # Sort files by modification time (most recent first)
    rir_files.sort(key=os.path.getmtime, reverse=True)
    newest_file = rir_files[0]
    
    print(f"\n-> Found newest RIR file: '{os.path.basename(newest_file)}'")
    return newest_file

# --- Main Execution ---
if __name__ == "__main__":
    
    try:
        import scipy.fft 
        import pandas as pd
    except ImportError:
        print("FATAL ERROR: The required library 'scipy' or 'pandas' is missing.")
        print("Please run: pip install scipy pandas openpyxl")
        exit()
        
    newest_rir_path = setup_and_prompt()
    
    if newest_rir_path is None:
        exit()
        
    print(f"\n--- Starting T60 Analysis on New File ---")
    
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
        print(f"Created output directory: '{OUTPUT_DIR}'")
    
    ref_chirp = generate_chirp_signal(SAMPLE_RATE, RECORD_SECONDS, F_START, F_END)
    
    # Process the single, newest file
    t60_results = []
    result = process_rir_file(newest_rir_path, ref_chirp) 
    if result is not None:
        t60_results.append(result)

    if t60_results:
        df_results = pd.DataFrame(t60_results)
        excel_output_path = os.path.join(OUTPUT_DIR, OUTPUT_EXCEL_FILE)
        
        # Merge with existing summary if it exists
        if os.path.exists(excel_output_path):
            existing_df = pd.read_excel(excel_output_path)
            df_results = pd.concat([existing_df, df_results], ignore_index=True)

        try:
            df_results.to_excel(excel_output_path, index=False)
            print(f"\n✅ Final T60 analysis complete.")
            print(f"The result for the shielded measurement is saved in '{excel_output_path}'.")
        except Exception as e:
            print(f"\nERROR: Could not save results to Excel. Ensure you have 'openpyxl' installed.")
            print(f"Error details: {e}")
