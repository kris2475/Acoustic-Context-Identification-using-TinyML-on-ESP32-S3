#include "Arduino.h"
#include <ESP_I2S.h>
#include <FS.h>
#include <SD.h>
#include "esp_heap_caps.h"
#include <SPI.h>
#include <cmath> 

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
#define RIR_VOLUME_GAIN          2.0 

// Passive Clip specific (Optimized for TinyML efficiency)
#define PASSIVE_CLIP_SECONDS     2
#define NUM_CLIPS_TO_RECORD      30
#define DELAY_BETWEEN_CLIPS_MS   2000 

// --- Buffer ---
#define BYTES_PER_SAMPLE 2
// Buffer size is set to the maximum required (RIR measurement)
#define RIR_BUFFER_SIZE_BYTES (SAMPLE_RATE * BYTES_PER_SAMPLE * RIR_RECORD_SECONDS)
uint8_t* audio_buffer = nullptr;

// --- Global I2S objects ---
I2SClass I2Sout;
I2SClass I2Sin;

// --- User Variables ---
String base_filename = "";
char base_path[64]; // Stores /LABEL_SESSIONID

// --- Prototypes ---
void generateChirp(int16_t* buf, size_t len);
void applyGain();
void saveWAV(const char* filename, size_t data_size);
void waitForUserConfirmation(const char* prompt);
void runDataAcquisitionSession();
void getUserInput();
void runRIRCapture();
void runPassiveCapture();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32-S3 Data Acquisition System ===");

  // 1. SD Card Initialization
  if(!SD.begin(SD_CS, SPI, 1000000)){
    Serial.println("ERROR: SD card init failed!");
    while(1);
  }
  Serial.println("SD Card ready.");

  // 2. Allocate buffer in PSRAM for the RIR measurement (the largest needed)
  audio_buffer = (uint8_t*)heap_caps_malloc(RIR_BUFFER_SIZE_BYTES, MALLOC_CAP_SPIRAM);
  if(!audio_buffer){
    Serial.println("ERROR: Failed to allocate PSRAM buffer!");
    while(1);
  }
  Serial.printf("Buffer allocated: %u bytes.\n", RIR_BUFFER_SIZE_BYTES);

  // 3. Start the main data acquisition session
  runDataAcquisitionSession();
}

void loop() {
  // All main logic runs once in setup() via runDataAcquisitionSession()
  delay(5000);
}

// --- Main Logic Flow ---

void runDataAcquisitionSession(){
  getUserInput(); 

  // Check if directory exists, if not, create it
  if (!SD.exists(base_path)) {
    SD.mkdir(base_path);
    Serial.printf("Created directory: %s\n", base_path);
  } else {
    Serial.printf("Using existing directory: %s\n", base_path);
  }

  // PHASE 1: ACTIVE RIR CAPTURE (Silence Required)
  Serial.println("Ensure the room is quiet and the Pink Noise is OFF.");
  waitForUserConfirmation("RIR MEASUREMENT: Ready to capture 5s Room Impulse Response.");
  runRIRCapture();

  // PHASE 2: PAUSE for Pink Noise Setup
  waitForUserConfirmation("PINK NOISE SETUP: The RIR measurement is complete. Please NOW start playing the CONTINUOUS PINK NOISE from your fixed-volume BT speaker.");

  // PHASE 3: PASSIVE DATA CAPTURE (Pink Noise Required)
  runPassiveCapture();

  Serial.println("==================================");
  Serial.println("Data collection complete.");
  Serial.println("==================================");
}

// --- Input & Control Functions ---

// Pauses execution until user presses a key
void waitForUserConfirmation(const char* prompt){
  Serial.println("----------------------------------");
  Serial.println(prompt);
  Serial.println(">>> PRESS ANY KEY AND HIT ENTER TO CONTINUE <<<");
  Serial.println("----------------------------------");
  while(Serial.available() == 0) { delay(100); }
  // Clear the input buffer after reading the character
  while(Serial.available() > 0) { Serial.read(); }
}

void getUserInput(){
  Serial.println("----------------------------------");
  Serial.println("--- Session Setup ---");
  
  // 1. Get Label (e.g., living_room_hard)
  Serial.println("Enter Label (NO slashes, colons, or spaces):");
  while(Serial.available() == 0) { delay(100); }
  String label = Serial.readStringUntil('\n');
  label.trim();
  
  // 2. Get Session ID (e.g., 20251121_144200)
  Serial.println("Enter Session ID (e.g., 20251121_144200 - NO slashes/colons/spaces):");
  while(Serial.available() == 0) { delay(100); }
  String session_id = Serial.readStringUntil('\n');
  session_id.trim();

  // Construct the base filename and directory path (now file-safe)
  base_filename = label + "_" + session_id;
  sprintf(base_path, "/%s", base_filename.c_str());
  
  Serial.printf("\nSession Label: %s\n", base_filename.c_str());
  Serial.printf("Saving to directory: %s\n", base_path);
  
  // Instructions for consistency
  Serial.println("----------------------------------");
  Serial.println("--- Passive Capture Setup Policy ---");
  Serial.println("1. Use the SAME Bluetooth speaker and placement for all rooms.");
  Serial.println("2. Pink Noise (or other long, continuous clip) must be used for passive capture.");
  Serial.println("3. Volume must be FIXED and CONSISTENT between all environments.");
  Serial.println("----------------------------------");
}

// --- RIR Capture Functions (Active Measurement) ---

void runRIRCapture(){
  Serial.println("\n--- 1. ACTIVE RIR MEASUREMENT STARTING ---");
  Serial.println("Generating reference chirp...");
  
  // Generate chirp directly in buffer for playback
  int16_t* samples = (int16_t*)audio_buffer;
  generateChirp(samples, RIR_BUFFER_SIZE_BYTES / 2);
  
  Serial.println("Starting simultaneous playback & record...");
  
  // Start I2S output (TX)
  I2Sout.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if(!I2Sout.begin(I2S_MODE_STD, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
    Serial.println("ERROR: I2S TX init failed!");
    return;
  }
  
  // Start I2S PDM input (RX)
  I2Sin.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if(!I2Sin.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
    Serial.println("ERROR: I2S PDM init failed!");
    I2Sout.end();
    return;
  }

  // Record & play simultaneously
  size_t bytes_played = 0;
  size_t bytes_recorded = 0;
  while(bytes_played < RIR_BUFFER_SIZE_BYTES){
    // Play
    size_t chunk = min((size_t)1024, RIR_BUFFER_SIZE_BYTES - bytes_played);
    size_t written = I2Sout.write(audio_buffer + bytes_played, chunk);
    if(written > 0) bytes_played += written;
    
    // Record
    chunk = min((size_t)1024, RIR_BUFFER_SIZE_BYTES - bytes_recorded);
    size_t read = I2Sin.readBytes((char*)(audio_buffer + bytes_recorded), chunk);
    if(read > 0) bytes_recorded += read;
  }

  I2Sout.end();
  I2Sin.end();
  Serial.println("Playback & recording finished.");

  // Apply gain
  applyGain();
  
  // Save RIR WAV file
  char filename[128];
  sprintf(filename, "%s/RIR_%s.wav", base_path, base_filename.c_str());
  saveWAV(filename, RIR_BUFFER_SIZE_BYTES);

  Serial.printf("RIR saved to %s\n", filename);
}


// --- Passive Capture Functions (Training Data) ---

void runPassiveCapture(){
  Serial.println("\n--- 3. PASSIVE DATA CAPTURE STARTING ---");
  Serial.printf("Recording %d x %d second passive clips from Pink Noise.\n", NUM_CLIPS_TO_RECORD, PASSIVE_CLIP_SECONDS);
  
  // Re-initialize I2S PDM input only
  I2Sin.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if(!I2Sin.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS)){
    Serial.println("ERROR: I2S PDM init failed for passive capture!");
    return;
  }

  // Buffer size uses the 2-second clip duration
  const size_t PASSIVE_BUFFER_SIZE = SAMPLE_RATE * BYTES_PER_SAMPLE * PASSIVE_CLIP_SECONDS;

  for(int i = 1; i <= NUM_CLIPS_TO_RECORD; i++){
    Serial.println("----------------------------------");
    // Updated prompt for user
    Serial.printf("Clip %d/%d (2s duration): Recording from continuous BT audio NOW...\n", i, NUM_CLIPS_TO_RECORD);
    
    // Clear buffer for this clip
    memset(audio_buffer, 0, PASSIVE_BUFFER_SIZE);
    
    // Record one clip
    size_t bytes_recorded = 0;
    while(bytes_recorded < PASSIVE_BUFFER_SIZE){
      size_t chunk = PASSIVE_BUFFER_SIZE - bytes_recorded;
      size_t read = I2Sin.readBytes((char*)(audio_buffer + bytes_recorded), chunk);
      if(read > 0) bytes_recorded += read;
    }
    
    // Save the clip with sequential filename
    char filename[128];
    sprintf(filename, "%s/PASSIVE_%s_%02d.wav", base_path, base_filename.c_str(), i);
    saveWAV(filename, PASSIVE_BUFFER_SIZE);
    Serial.printf("Saved %s.\n", filename);

    // --- Delay Between Clips ---
    Serial.printf("Waiting %d seconds before next capture...\n", DELAY_BETWEEN_CLIPS_MS / 1000);
    delay(DELAY_BETWEEN_CLIPS_MS); 
  }
  
  I2Sin.end();
}

// --- Utility Functions ---

void generateChirp(int16_t* buf, size_t len){
  // Generates 500Hz to 4000Hz linear frequency sweep (corrected)
  float f0 = 500.0; 
  float f1 = 4000.0;
  float T = (float)RIR_RECORD_SECONDS; // 5.0 seconds
  // K is the rate of frequency change: (f1 - f0) / T
  float K = (f1 - f0) / T;

  for(size_t i=0;i<len;i++){
    // *** FIXED ***: Calculate time 't' correctly in seconds.
    float t = (float)i / SAMPLE_RATE; 
    
    // Phase calculation for a linear frequency sweep: integral of 2*pi*f(t)
    // f(t) = f0 + K * t
    // Phase = 2*pi * (f0*t + (K/2)*t^2)
    float phase = 2.0 * PI * (f0 * t + (K / 2.0) * t * t);
    
    // Apply sine wave and scale to 16-bit
    buf[i] = (int16_t)(32767.0 * sin(phase));
  }
}

void applyGain(){
  // Applies gain to the RIR recording
  Serial.printf("Applying gain of %.1f...\n", RIR_VOLUME_GAIN);
  int16_t* samples = (int16_t*)audio_buffer;
  for(size_t i=0;i<RIR_BUFFER_SIZE_BYTES/2;i++){
    int32_t s = (int32_t)samples[i] * RIR_VOLUME_GAIN;
    if(s>32767) s=32767;
    if(s<-32768) s=-32768;
    samples[i] = (int16_t)s;
  }
}

void saveWAV(const char* filename, size_t data_size){
  Serial.printf("Saving WAV: %s\n", filename);
  // Attempt to open the file. This should now succeed with file-safe names.
  File f = SD.open(filename, FILE_WRITE);
  if(!f){
    Serial.println("ERROR: Cannot open file");
    return;
  }

  uint32_t datasize = data_size;
  uint32_t samplerate = SAMPLE_RATE;
  uint16_t bits = 16;
  uint16_t channels = 1;

  // --- RIFF header ---
  f.write((const uint8_t*)"RIFF",4);
  uint32_t chunkSize = 36 + datasize;
  f.write((uint8_t*)&chunkSize,4);
  f.write((const uint8_t*)"WAVE",4);

  // --- fmt chunk ---
  f.write((const uint8_t*)"fmt ",4);
  uint32_t fmtSize = 16;
  f.write((uint8_t*)&fmtSize,4);
  uint16_t audioFormat = 1;
  f.write((uint8_t*)&audioFormat,2);
  f.write((uint8_t*)&channels,2);
  f.write((uint8_t*)&samplerate,4);
  uint32_t byteRate = SAMPLE_RATE * channels * bits / 8;
  f.write((uint8_t*)&byteRate,4);
  uint16_t blockAlign = channels * bits / 8;
  f.write((uint8_t*)&blockAlign,2);
  f.write((uint8_t*)&bits,2);

  // --- data chunk ---
  f.write((const uint8_t*)"data",4);
  f.write((uint8_t*)&datasize,4);
  f.write(audio_buffer, datasize);

  f.close();
  Serial.println("WAV file saved.");
}




