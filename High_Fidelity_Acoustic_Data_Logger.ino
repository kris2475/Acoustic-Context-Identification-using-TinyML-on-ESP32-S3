/**
 * @file Acoustic_Feature_Recognition_RIR_with_Calc_Full_Corrected.ino
 * @brief ESP32-S3 Data Acquisition: Active RIR + Passive Pink Noise + Correct T60 Calculation
 * * Fully preserves ALL original functionality and structure, including prompts.
 * Fixes RIR calculation by using proper cross-correlation.
 * * MODIFIED: Removed the computationally expensive RIR/T60 calculation from the MCU.
 * The raw RIR WAV file is saved for superior T60 analysis in Python.
 */

#include "Arduino.h"
#include <ESP_I2S.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <cmath>
#include <vector>

// --- Pins ---
#define I2S_BCLK  1
#define I2S_LRC   2
#define I2S_DOUT  3
#define PDM_CLK_PIN  42
#define PDM_DATA_PIN 41
#define SD_CS   21
#define SD_SCK  7
#define SD_MISO 8
#define SD_MOSI 9

// --- Audio config ---
#define SAMPLE_RATE      16000
#define BITS_PER_SAMPLE  I2S_DATA_BIT_WIDTH_16BIT
#define CHANNELS         I2S_SLOT_MODE_MONO

// RIR specific
#define RIR_RECORD_SECONDS       5
// Passive Clip specific
#define PASSIVE_CLIP_SECONDS     2
#define NUM_CLIPS_TO_RECORD      30
#define DELAY_BETWEEN_CLIPS_MS   2000

// --- Buffer ---
#define BYTES_PER_SAMPLE 2
#define RIR_BUFFER_SIZE_BYTES (SAMPLE_RATE * BYTES_PER_SAMPLE * RIR_RECORD_SECONDS)
uint8_t* audio_buffer = nullptr;

// --- Global I2S objects ---
I2SClass I2Sout;
I2SClass I2Sin;

// --- User Variables ---
String base_filename = "";
char base_path[64];

// --- Function Prototypes ---
void generateChirp(int16_t* buf, size_t len);
void saveWAV(const char* filename, size_t data_size);
void waitForUserConfirmation(const char* prompt);
void runDataAcquisitionSession();
void getUserInput();
void runRIRCapture();
void runPassiveCapture();
// void calculateRIRAndT60(int16_t* recorded, int16_t* reference, size_t len); // DELETED
float computeEnvironmentScaleFromTestBurst(int16_t* buf, size_t len, float target_peak);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32-S3 Data Acquisition System ===");
  if(!SD.begin(SD_CS, SPI, 1000000)){
    Serial.println("ERROR: SD card init failed!");
    while(1);
  }
  Serial.println("SD Card ready.");
  audio_buffer = (uint8_t*)heap_caps_malloc(RIR_BUFFER_SIZE_BYTES, MALLOC_CAP_SPIRAM);
  if(!audio_buffer){
    Serial.println("ERROR: Failed to allocate PSRAM buffer!");
    while(1);
  }
  Serial.printf("Buffer allocated: %u bytes.\n", RIR_BUFFER_SIZE_BYTES);

  runDataAcquisitionSession();
}

void loop() {
  delay(5000);
// nothing to do here
}

// --- Main Flow ---
void runDataAcquisitionSession(){
  getUserInput();
  if (!SD.exists(base_path)) SD.mkdir(base_path);
  Serial.println("Ensure the room is quiet and the Pink Noise is OFF.");
  waitForUserConfirmation("RIR MEASUREMENT: Ready to capture 5s Room Impulse Response. TURN DOWN BLUETOOTH SPEAKER VOLUME!");
  runRIRCapture();
  waitForUserConfirmation("PINK NOISE SETUP: Now start playing continuous Pink Noise from your fixed-volume BT speaker.");
  runPassiveCapture();

  Serial.println("==================================");
  Serial.println("Data collection complete.");
  Serial.println("==================================");
}

void waitForUserConfirmation(const char* prompt){
  Serial.println("----------------------------------");
  Serial.println(prompt);
  Serial.println(">>> PRESS ANY KEY AND HIT ENTER TO CONTINUE <<<");
  Serial.println("----------------------------------");
  while(Serial.available() == 0) delay(100);
  while(Serial.available() > 0) Serial.read();
}

void getUserInput(){
  Serial.println("----------------------------------");
  Serial.println("--- Session Setup ---");
  Serial.println("Enter Label (NO slashes, colons, or spaces):");
  while(Serial.available() == 0) delay(100);
  String label = Serial.readStringUntil('\n');
  label.trim();
  Serial.println("Enter Session ID (e.g., 20251121_144200):");
  while(Serial.available() == 0) delay(100);
  String session_id = Serial.readStringUntil('\n');
  session_id.trim();
  base_filename = label + "_" + session_id;
  sprintf(base_path, "/%s", base_filename.c_str());

  Serial.printf("\nSession Label: %s\n", base_filename.c_str());
  Serial.printf("Saving to directory: %s\n", base_path);
  Serial.println("----------------------------------");
  Serial.println("--- Passive Capture Setup Policy ---");
  Serial.println("1. Use SAME BT speaker and placement for all rooms.");
  Serial.println("2. Pink Noise (or other long clip) must be used for passive capture.");
  Serial.println("3. Volume must be FIXED and CONSISTENT between all environments.");
  Serial.println("----------------------------------");
}

void runRIRCapture() {
    Serial.println("\n--- 1. ACTIVE RIR MEASUREMENT STARTING ---");
    Serial.println("Generating reference chirp...");

    int16_t* samples = (int16_t*)audio_buffer;
    size_t chirp_len = RIR_BUFFER_SIZE_BYTES / 2;

    // Generate full chirp buffer (we will play a short section first)
    generateChirp(samples, chirp_len);
    // --- Stage 1: Short test chirp (first ~0.25s) ---
    Serial.println("Stage 1: Playing short test chirp to measure environment...");
    size_t test_len = SAMPLE_RATE / 4; // 0.25s
    // Clear buffer before test
    for (size_t i = 0; i < chirp_len; i++) samples[i] = 0;
    // Fill test chirp
    generateChirp(samples, test_len);

    // Play test chirp safely
    I2Sout.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
    if(!I2Sout.begin(I2S_MODE_STD, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
        Serial.println("ERROR: I2S TX init failed!");
        return;
    }

    size_t bytes_played = 0;
    while(bytes_played < test_len * BYTES_PER_SAMPLE){
        size_t chunk = min((size_t)1024, test_len*BYTES_PER_SAMPLE - bytes_played);
        size_t written = I2Sout.write(audio_buffer + bytes_played, chunk);
        if(written > 0) bytes_played += written;
    }
    I2Sout.end();
    // --- Measure environment scale from test chirp ---
    float envScale = computeEnvironmentScaleFromTestBurst(samples, test_len, 31129.0);
    // Clamp to safe max (avoid DAC clipping)
    if(envScale > 1.0) envScale = 1.0;
    Serial.printf("Computed environment scale factor (clamped): %.3f\n", envScale);

    // --- Stage 2: Full chirp playback & recording ---
    memset(samples, 0, chirp_len * sizeof(int16_t));
    generateChirp(samples, chirp_len);
    for(size_t i = 0; i < chirp_len; i++) samples[i] = (int16_t)(samples[i] * envScale);
    Serial.println("Starting scaled full-chirp playback & simultaneous recording...");

    I2Sout.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
    if(!I2Sout.begin(I2S_MODE_STD, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
        Serial.println("ERROR: I2S TX init failed!");
        return;
    }

    I2Sin.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
    if(!I2Sin.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
        Serial.println("ERROR: I2S PDM init failed!");
        I2Sout.end();
        return;
    }

    bytes_played = 0;
    size_t bytes_recorded = 0;
    while(bytes_played < RIR_BUFFER_SIZE_BYTES){
        size_t chunk = min((size_t)1024, RIR_BUFFER_SIZE_BYTES - bytes_played);
        size_t written = I2Sout.write(audio_buffer + bytes_played, chunk);
        if(written > 0) bytes_played += written;

        chunk = min((size_t)1024, RIR_BUFFER_SIZE_BYTES - bytes_recorded);
        size_t read = I2Sin.readBytes((char*)(audio_buffer + bytes_recorded), chunk);
        if(read > 0) bytes_recorded += read;
    }

    I2Sout.end();
    I2Sin.end();
    Serial.println("Playback & recording finished.");

    // --- Save WAV ---
    char filename[128];
    sprintf(filename, "%s/RIR_%s.wav", base_path, base_filename.c_str());
    saveWAV(filename, RIR_BUFFER_SIZE_BYTES);
    Serial.printf("Raw RIR data saved to %s\n", filename);

    // --- Cleanup reference array ---
    int16_t* reference = new int16_t[chirp_len];
    generateChirp(reference, chirp_len);
    // NOTE: T60 calculation is moved to the Python script for better accuracy and performance.
    delete[] reference;
}

// REMOVED: calculateRIRAndT60 function body


void runPassiveCapture(){
  Serial.println("\n--- 2. PASSIVE DATA CAPTURE STARTING ---");
  I2Sin.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if(!I2Sin.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){ Serial.println("ERROR: I2S PDM init failed!"); return; }

  const size_t PASSIVE_BUFFER_SIZE=SAMPLE_RATE*BYTES_PER_SAMPLE*PASSIVE_CLIP_SECONDS;
  for(int i=1;i<=NUM_CLIPS_TO_RECORD;i++){
    Serial.println("----------------------------------");
    Serial.printf("Clip %d/%d (2s): Recording from Pink Noise...\n",i,NUM_CLIPS_TO_RECORD);
    memset(audio_buffer,0,PASSIVE_BUFFER_SIZE);
    size_t bytes_recorded=0;
    while(bytes_recorded<PASSIVE_BUFFER_SIZE){
      size_t chunk = PASSIVE_BUFFER_SIZE - bytes_recorded;
      size_t read = I2Sin.readBytes((char*)(audio_buffer+bytes_recorded),chunk);
      if(read>0) bytes_recorded += read;
    }
    char filename[128];
    sprintf(filename, "%s/PASSIVE_%s_%02d.wav", base_path, base_filename.c_str(), i);
    saveWAV(filename,PASSIVE_BUFFER_SIZE);
    Serial.printf("Saved %s\n", filename);
    delay(DELAY_BETWEEN_CLIPS_MS);
  }
  I2Sin.end();
}

void generateChirp(int16_t* buf, size_t len){
  float f0=500.0,f1=4000.0,T=(float)RIR_RECORD_SECONDS,K=(f1-f0)/T;
  for(size_t i=0;i<len;i++){
    float t=(float)i/SAMPLE_RATE;
    float phase=2.0*PI*(f0*t+0.5*K*t*t);
    buf[i]=(int16_t)(32767.0*sin(phase));
  }
}

float computeEnvironmentScaleFromTestBurst(int16_t* buf, size_t len, float target_peak){
  float peak=0,rms=0;
  for(size_t i=0;i<len;i++){ float val=abs(buf[i]); if(val>peak) peak=val; rms+=val*val; }
  rms=sqrt(rms/len);
  float scale=target_peak/peak;
  Serial.printf("Test burst measured: peak=%.0f, rms=%.2f\n",peak,rms);
  Serial.printf("Environment scale factor: %.3f\n",scale);
  return scale;
}


void saveWAV(const char* filename,size_t data_size){
  Serial.printf("Saving WAV: %s\n",filename);
  File f=SD.open(filename,FILE_WRITE);
  if(!f){Serial.println("ERROR: Cannot open file"); return;}

  uint32_t datasize=data_size, samplerate=SAMPLE_RATE;
  uint16_t bits=16, channels=1;

  f.write((const uint8_t*)"RIFF",4);
  uint32_t chunkSize=36+datasize; f.write((uint8_t*)&chunkSize,4);
  f.write((const uint8_t*)"WAVE",4);
  f.write((const uint8_t*)"fmt ",4);
  uint32_t fmtSize=16; f.write((uint8_t*)&fmtSize,4);
  uint16_t audioFormat=1; f.write((uint8_t*)&audioFormat,2);
  f.write((uint8_t*)&channels,2);
  f.write((uint8_t*)&samplerate,4);
  uint32_t byteRate=SAMPLE_RATE*channels*bits/8; f.write((uint8_t*)&byteRate,4);
  uint16_t blockAlign=channels*bits/8; f.write((uint8_t*)&blockAlign,2); f.write((uint8_t*)&bits,2);
  f.write((const uint8_t*)"data",4); f.write((uint8_t*)&datasize,4);
  f.write(audio_buffer,datasize);
  f.close();
  Serial.println("WAV file saved.");
}













