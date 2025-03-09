#include <Arduino.h>
#include <Wire.h>
 
#define MPU_ADDR 0x68   // I2C address for the MPU6050
 
// Global calibration parameters
float ax_offset = 0.0;      // Offset determined from neutral position
float neg_scale = 1.0;      // Scale factor for negative values
 
// Wake up MPU6050
void setupMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // Power management register
  Wire.write(0);      // Set to 0 (wakes up the MPU6050)
  Wire.endTransmission(true);
}
 
// Read raw 16-bit value from the accelerometer's X-axis (registers 0x3B & 0x3C)
int16_t readAccelX() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);   // Starting register for ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2, true);
  int16_t highByte = Wire.read();
  int16_t lowByte = Wire.read();
  return (highByte << 8) | lowByte;
}
 
// Wait for user input via serial and then flush it
void waitForUser(const char *msg) {
  Serial.println(msg);
  while (Serial.available() == 0) {
    // Wait for a key press
  }
  // Flush any available data
  while (Serial.available() > 0) {
    Serial.read();
  }
}
 
// Calculate offset in a neutral position using multiple samples
void calibrateOffset() {
  waitForUser("Place the robot in its neutral position and press any key to continue...");
 
  const int numSamples = 100;
  long sum = 0;
  for (int i = 0; i < numSamples; i++) {
    sum += readAccelX();
    delay(10);
  }
  ax_offset = sum / (float) numSamples;
  Serial.print("Calibrated offset (X): ");
  Serial.println(ax_offset);
}
 
// Sample sensor readings in a specific orientation (e.g., "nose down" or "back down")
float sampleOrientation(const char *orientation) {
  String prompt = "Place the robot in ";
  prompt += orientation;
  prompt += " orientation and press any key to continue...";
  waitForUser(prompt.c_str());
 
  const int numSamples = 100;
  long sum = 0;
  for (int i = 0; i < numSamples; i++) {
    sum += readAccelX();
    delay(10);
  }
  float avg = sum / (float) numSamples;
  Serial.print(orientation);
  Serial.print(" average raw reading: ");
  Serial.println(avg);
  return avg;
}
 
// Compute a scale factor for negative readings so that after subtracting offset,
// the magnitude in the "back down" position matches the "nose down" reading.
void calibrateNegativeScale() {
  // Collect readings for two known orientations:
  float noseAvg = sampleOrientation("nose down"); // Expect positive reading
  float backAvg = sampleOrientation("back down"); // Expect negative reading
 
  // Correct for offset
  float noseCorrected = noseAvg - ax_offset;
  float backCorrected = backAvg - ax_offset;
 
  Serial.print("Nose corrected value: ");
  Serial.println(noseCorrected);
  Serial.print("Back corrected value: ");
  Serial.println(backCorrected);
 
  // Compute scale factor only if the back reading is negative.
  if (backCorrected < 0) {
    // Scale factor to make |back_corrected| equal to nose_corrected
    neg_scale = noseCorrected / (-backCorrected);
  } else {
    neg_scale = 1.0; // Fallback in case of an unexpected positive reading
  }
  Serial.print("Computed negative scale factor: ");
  Serial.println(neg_scale);
}
 
// Function to return calibrated acceleration along the X-axis.
// First subtract the offset and then apply scaling for negative values.
float getCalibratedAccelX() {
  int16_t raw = readAccelX();
  float corrected = (raw - ax_offset);
  if (corrected < 0) {
    corrected *= neg_scale;
  }
  return corrected;
}
 
void setup() {
  Serial.begin(57600);
  Wire.begin();
  setupMPU();
  delay(100); // Let sensor settle
 
  Serial.println("=== MPU6050 Calibration Program ===");
 
  // Calibrate offset in neutral position
  calibrateOffset();
 
  // Calibrate negative scale factor using known orientations
  calibrateNegativeScale();
}
 
void loop() {
  long tot = 0;
  for ( int i=1; i<= 100 ; i++){
  // Continuously read and print the calibrated X-axis acceleration
  float ax = getCalibratedAccelX();
  tot += ax;
  Serial.print("Calibrated Accel X: ");
  Serial.print(round(ax));

  Serial.print("   avg: ");
  Serial.println(round(tot/i));
  delay(500);
  }
}
 