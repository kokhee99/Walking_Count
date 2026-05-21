// Core libraries: Arduino framework and I2C support
#include <Arduino.h>                // Arduino core functions and types
#include <Wire.h>                   // TwoWire I2C library used for sensors and displays

// Storage and peripheral libraries
#include <Preferences.h>            // ESP32 NVS preferences for persistent storage
#include <U8g2lib.h>               // U8g2 graphics library for OLED displays
#include "LIS3DHTR.h"              // LIS3DHTR driver for 3-axis accelerometer

// Display dimensions (not actively used by U8g2 API but kept for reference)
#define SCREEN_WIDTH 128           // OLED width in pixels
#define SCREEN_HEIGHT 64           // OLED height in pixels

// NVS namespace and key used to save the step count persistently
const char* PREF_NAMESPACE = "stepcounter"; // NVS namespace for our app data
const char* PREF_KEY = "count";            // NVS key name to store step count

// U8g2 OLED object for SSD1306 128x64 using hardware I2C, no reset pin
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// LIS3DHTR accelerometer object; initialization uses Wire in setup
LIS3DHTR<TwoWire> lis3dhtr;

// Preferences object for non-volatile storage
Preferences preferences;

// Runtime state variables
unsigned long stepCount = 0;          // Running total of detected steps
unsigned long lastStepTime = 0;       // Timestamp (ms) of the last counted step
unsigned long lastSaveTime = 0;       // Timestamp (ms) of the last NVS write
float filteredAccel = 0.0f;           // Filtered linear acceleration value
bool stepReady = false;               // Internal flag indicating a candidate step

// Algorithm tuning constants
const float alpha = 0.90f;            // Low-pass filter smoothing factor (0..1)
const float stepThreshold = 0.35f;    // Threshold above which a step "rises"
const uint32_t minStepIntervalMs = 250; // Minimum ms between counted steps (debounce)
const uint32_t saveIntervalMs = 10000;  // Save to NVS at least this often (ms)

// Draw the main screen: title + large count + footer
void showDisplay(const char* title, unsigned long count) {
  char countText[16];                  // Buffer to hold count as text
  sprintf(countText, "%lu", count);  // Convert numeric count into ASCII

  u8g2.clearBuffer();                  // Clear internal U8g2 drawing buffer
  u8g2.setFont(u8g2_font_ncenB08_tr);  // Choose a readable small font for title
  u8g2.drawStr(0, 12, title);          // Draw title at (x=0, y=12)

  u8g2.setFont(u8g2_font_fub20_tr);    // Select a bold large font for the count
  u8g2.drawStr(0, 45, countText);      // Draw the step count text near the center

  u8g2.setFont(u8g2_font_courR08_tr);  // Small monospace font for footer
  u8g2.drawStr(0, 62, "Steps");       // Draw footer label at the bottom row
  u8g2.sendBuffer();                   // Send the composed buffer to the display
}

// Show an error message on the OLED (two lines)
void showError(const char* line1, const char* line2) {
  u8g2.clearBuffer();                  // Clear buffer before drawing error
  u8g2.setFont(u8g2_font_ncenB08_tr);  // Use clear font for error lines
  u8g2.drawStr(0, 20, line1);          // Draw first error line at y=20
  u8g2.drawStr(0, 40, line2);          // Draw second error line at y=40
  u8g2.sendBuffer();                   // Push the error text to the OLED
}

// Save the current step count to non-volatile storage (NVS)
void saveStepCount(bool force = false) {
  unsigned long now = millis();        // Read current time in milliseconds
  if (!force && (now - lastSaveTime) < saveIntervalMs) { // If not forced and save interval not reached
    return;                             // Skip saving to avoid excessive writes
  }
  preferences.putULong(PREF_KEY, stepCount); // Write the unsigned long to NVS
  lastSaveTime = now;                   // Update the last save timestamp
}

// Setup runs once after reset/power-on
void setup() {
  Wire.begin(21, 22);                   // Initialize I2C with SDA=21, SCL=22
  Serial.begin(115200);                 // Initialize Serial at 115200 baud for debug
  while (!Serial && millis() < 2000) {  // Wait briefly for Serial port (if required)
    delay(10);                          // Short delay while waiting for Serial
    Serial.println("Waiting for initialising Serial Monitoring");
  }

  u8g2.begin();                         // Initialize the U8g2 OLED library
  u8g2.clearBuffer();                   // Clear any residual content in the buffer
  u8g2.setFont(u8g2_font_ncenB08_tr);   // Set font for the splash text
  u8g2.drawStr(0, 12, "XIAO ESP32S3 Step"); // Draw first line of splash
  u8g2.drawStr(0, 30, "Counter");     // Draw second line of splash
  u8g2.drawStr(0, 50, "Initializing..."); // Draw initialization message
  u8g2.sendBuffer();                    // Show splash on the OLED

  lis3dhtr.begin(Wire, 0x19);               // Initialize LIS3DHTR on I2C
  if (!lis3dhtr.isConnection()) {       // Verify the sensor is reachable
    Serial.println("Failed to connect to LIS3DHTR"); // Log failure on Serial
    showError("LIS3DHTR not found", "Check wiring"); // Show error on OLED
    while (true) {                      // Enter infinite loop to halt further execution
      delay(100);                       // Wait inside halt loop
    }
  }

  lis3dhtr.setFullScaleRange(LIS3DHTR_RANGE_2G); // Configure accelerometer range
  lis3dhtr.setOutputDataRate(LIS3DHTR_DATARATE_25HZ);  // Configure data rate

  preferences.begin(PREF_NAMESPACE, false); // Open NVS namespace for reading/writing
  stepCount = preferences.getULong(PREF_KEY, 0); // Load saved step count (default 0)
  lastSaveTime = millis();               // Initialize last save time to now

  Serial.println("Setup complete");    // Log successful setup
  Serial.print("Loaded step count: "); // Label the next Serial output
  Serial.println(stepCount);            // Print the loaded step count

  showDisplay("Steps", stepCount);     // Display the loaded step count on the OLED
  delay(2000);                          // Keep splash visible for 2 seconds
}

// Main loop runs repeatedly
void loop() {
  float ax = 0.0f;                     // X-axis acceleration placeholder
  float ay = 0.0f;                     // Y-axis acceleration placeholder
  float az = 0.0f;                     // Z-axis acceleration placeholder
  lis3dhtr.getAcceleration(&ax, &ay, &az); // Read acceleration values into variables

  float magnitude = sqrt(ax * ax + ay * ay + az * az); // Compute vector magnitude
  float linearAccel = magnitude - 1.0f; // Subtract 1g gravity to get linear acceleration
  filteredAccel = alpha * filteredAccel + (1.0f - alpha) * linearAccel; // Apply low-pass filter

  unsigned long now = millis();         // Current timestamp for timing logic
  if (filteredAccel > stepThreshold && !stepReady && (now - lastStepTime) > minStepIntervalMs) {
    stepReady = true;                   // Mark that we've seen a rising edge above threshold
  }

  if (filteredAccel < 0.15f && stepReady) { // Detect falling edge to confirm step completion
    stepCount++;                         // Increment the persistent step counter
    lastStepTime = now;                  // Update timestamp of last detected step
    stepReady = false;                   // Reset the step-ready flag for next detection
    saveStepCount();                     // Attempt to save to NVS (subject to save interval)
    Serial.print("Step: ");            // Log step event label
    Serial.println(stepCount);           // Log current step count value
  }

  if (now - lastSaveTime >= saveIntervalMs) { // Periodic forced save check
    saveStepCount(true);                 // Force-save to persist count immediately
  }

  showDisplay("Steps", stepCount);     // Update OLED with latest step count every loop
  delay(30);                            // Small loop delay to stabilize sampling rate
}
