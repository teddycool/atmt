#include <Wire.h>
#include <Arduino.h>

#define MCP23017_ADDR 0x20 // Default I2C address for MCP23017
#define TCA9548_ADDR 0x70  // I2C address for TCA9548 (multiplexer)
#define PCA9685_ADDR 0x40  // Default I2C address for PCA9685

// 9685 servo driver
#define MODE1_REG 0x00      // Mode1 register address
#define PRESCALE_REG 0xFE   // Prescale register address (to set PWM frequency)
#define LED0_ON_L_REG 0x06  // LED0 On Low register (for channel 0)
#define LED0_OFF_L_REG 0x08 // LED0 Off Low register (for channel 0)

// MCP23017 Registers
#define IODIRA 0x00 // I/O Direction Register (0 = Output, 1 = Input)
#define IODIRB 0x01 // GPIO B direction register
#define GPIOA 0x12  // GPIO A register (read/write output)
#define GPIOB 0x13  // GPIO B register (read/write output)
#define OLATA 0x14  // GPIO A output latch register
#define OLATB 0x15  // GPIO B output latch register

// PCA9685 Registers
#define MODE1 0x00 // Mode1 Register (used for setting the operation mode)
#define MODE2 0x01 // Mode2 Register (used for setting the output mode)

#define VL53L0X_ADDR 0x29  // Default I2C address of VL53L0X



void selectPCA9548Channel(uint8_t channel)
{
  // The PCA9548 allows selecting channels via a 1-byte command
  // Each bit in the byte corresponds to a channel (0 = disabled, 1 = enabled)

  if (channel > 7)
  {
    Serial.println("Invalid channel selection. Channel must be between 0 and 7.");
    return;
  }

  // Construct the command byte. We are enabling only the specified channel.
  uint8_t command = 1 << channel; // Left shift 1 by 'channel' bits (e.g., for channel 0, command = 0x01)

  Wire.beginTransmission(TCA9548_ADDR); // Start communication with PCA9548
  Wire.write(command);                  // Send the command byte to select the channel
  Wire.endTransmission();               // End transmission

  Serial.print("Selected channel: ");
  Serial.println(channel);
}

void initVL53L0x(uint8_t channel) {
  selectPCA9548Channel(channel);
  Wire.beginTransmission(VL53L0X_ADDR);
  Wire.write(0x80);  // Write to system initialization register
  Wire.write(0x01);  // Set bit 0 to 1 to initialize
  Wire.endTransmission();
  delay(10);

    // Set measurement timing budget (e.g., 20ms)
    Wire.beginTransmission(VL53L0X_ADDR);
    Wire.write(0x04);  // Timing Budget register
    Wire.write(0x0A);  // Set to 20ms
    Wire.endTransmission();
  
    // Set inter-measurement period (e.g., 50ms)
    Wire.beginTransmission(VL53L0X_ADDR);
    Wire.write(0x03);  // Inter-Measurement Period register
    Wire.write(0x32);  // Set to 50ms
    Wire.endTransmission();
  
    delay(10);  // Wait for settings to take effect
  selectPCA9548Channel(0);

  delay(100); // Wait for sensor to initialize
}

uint16_t readVL53L0XDistance(uint8_t channel) {
  selectPCA9548Channel(channel);
  uint16_t distance = 0;
  Wire.beginTransmission(VL53L0X_ADDR);
  Wire.write(0x01); // Command to read distance
  Wire.endTransmission();
  Wire.requestFrom(VL53L0X_ADDR, 2);
  if (Wire.available() == 2) {
    distance = Wire.read() << 8;
    distance |= Wire.read();
  }
  selectPCA9548Channel(0);
  return distance;
}

// Function to set the frequency of the PCA9685
void setPWMFrequency(float frequency)
{
  uint8_t prescaleValue;
  float prescale = 25000000.0; // Default PCA9685 clock frequency (25 MHz)
  prescale /= 4096.0;          // Divide by the total number of steps (12-bit resolution)
  prescale /= frequency;       // Divide by desired frequency
  prescale -= 1.0;             // Subtract 1 from the value

  prescaleValue = floor(prescale + 0.5); // Round to the nearest integer

  // Step 1: Enter Sleep Mode by setting SLEEP bit to 1
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(MODE1_REG); // Write to the Mode1 register
  Wire.write(0x10);      // Set SLEEP bit to 1 to enter sleep mode temporarily
  Wire.endTransmission();

  // Step 2: Set the prescale register to configure the frequency
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(PRESCALE_REG);  // Write to the prescale register
  Wire.write(prescaleValue); // Write the calculated prescale value
  Wire.endTransmission();

  // Step 3: Wake up the PCA9685 by setting the SLEEP bit back to 0
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(MODE1_REG); // Write to the Mode1 register
  Wire.write(0x80);      // Wake up the chip (set SLEEP bit to 0) and restart oscillator
  Wire.endTransmission();
}

// Function to set the pulse width (servo position) for a channel
void setServoPulse(uint8_t channel, float pulseWidth)
{
  uint16_t pulse = pulseWidth * 4096 / 20000; // Convert ms to 12-bit PWM value

  // Calculate ON and OFF times
  uint8_t onLow = 0;                     // Set ON time low byte
  uint8_t offLow = pulse & 0xFF;         // Set OFF time low byte
  uint8_t offHigh = (pulse >> 8) & 0xFF; // Set OFF time high byte

  // Set the ON time for the channel (always 0)
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(LED0_ON_L_REG + 4 * channel); // LED channel start at 0x06 for channel 0
  Wire.write(onLow);
  Wire.write(0); // ON High is always 0
  Wire.write(offLow);
  Wire.write(offHigh);
  Wire.endTransmission();
}

// Write a value to the specified MCP23017 register
void mcp23017_write_register(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(MCP23017_ADDR);
  Wire.write(reg);   // Register address
  Wire.write(value); // Data to write
  Wire.endTransmission();
}

// Initialize the MCP23017
void mcp23017_init()
{
  // Set all pins on GPIOA and GPIOB to output (IODIRA = 0x00 for all outputs)
  mcp23017_write_register(IODIRA, 0x00); // Set all GPA pins to outputs
  mcp23017_write_register(IODIRB, 0x00); // Set all GPB pins to outputs

  // Make sure the output latch is cleared initially (set all pins to low)
  mcp23017_write_register(OLATA, 0x00); // Clear all GPA outputs
  mcp23017_write_register(OLATB, 0x00); // Clear all GPB outputs
}

void check_mcp23017_health()
{
  // Read the IODIRA register
  Wire.beginTransmission(MCP23017_ADDR);
  Wire.write(IODIRA); // Send register address (IODIRA)
  Wire.endTransmission();

  Wire.requestFrom(MCP23017_ADDR, 1); // Request 1 byte of data
  if (Wire.available())
  {
    uint8_t value = Wire.read(); // Read the value from the register
    Serial.print("IODIRA register value: 0x");
    Serial.println(value, HEX);
  }
  else
  {
    Serial.println("Failed to read from MCP23017.");
  }
}

// Write a value to the GPIOA register to control the pins
void mcp23017_write_GPIOA(uint8_t value)
{
  mcp23017_write_register(GPIOA, value);
}

void setup()
{
  Serial.begin(57600); // Start serial communication
  Wire.begin();        // Initialize I2C communication
  // Test connection with TCA9548 (I2C multiplexer)
  Wire.beginTransmission(TCA9548_ADDR);
  byte error = Wire.endTransmission(); // Check if the TCA9548 is responding
  if (error != 0)
  {
    Serial.print("TCA9548 I2C Communication Error: ");
    Serial.println(error);
    return;
  }
  Serial.println("TCA9548 found!");

  selectPCA9548Channel(0);

  // Test connection with PCA9685 (reading MODE1 register)
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(MODE1);              // Write the MODE1 register to read it
  error = Wire.endTransmission(); // Check if the I2C communication went through
  if (error != 0)
  {
    Serial.print("PCA9685 I2C Communication Error: ");
    Serial.println(error);
    return;
  }

  Wire.requestFrom(PCA9685_ADDR, 1); // Request 1 byte from the MODE1 register
  if (Wire.available())
  {
    byte mode1Value = Wire.read(); // Read the value of the MODE1 register
    Serial.print("MODE1 Register: 0x");
    Serial.println(mode1Value, HEX);

    // Check if the PCA9685 is in normal mode (MODE1 register should be 0x01 for normal operation)
    if (mode1Value == 0x01)
    {
      Serial.println("PCA9685 is connected and operating in normal mode!");
    }
    else
    {
      Serial.println("Unexpected MODE1 value. Check the connection.");
    }
  }
  else
  {
    Serial.println("Failed to read MODE1 register.");
  }

  // Step 1: Enable oscillator (OSCON bit) by setting SLEEP bit to 1 temporarily
  // This is done in the setPWMFrequency function (via sleep mode and waking it up)

  // Step 2: Set PWM frequency to 50 Hz (typical for servos)
  setPWMFrequency(1000.0); // Set to 50 Hz for servos

  // Step 3: Set servo position on channel 0
  // Neutral position (1.5ms pulse width)
  setServoPulse(0, 1.5); // 1.5 ms pulse width for neutral position
setServoPulse(15,500); // 500 ms ousle for blink (requires higher pwm frequency)
  Serial.println("PCA9685 setup complete.");

  // setup 23017 IO expander
  mcp23017_init();
  check_mcp23017_health();

  //init laser distance
  initVL53L0x(1);
}

void loop()
{
  // Example: Move servo to various positions
  // Turn LED on (set GPA7 high)
  Serial.print("[");
  Serial.print(readVL53L0XDistance(1));
  Serial.println("]");
  mcp23017_write_GPIOA(0x80); // 0x80 is bit 7 high (GPA7 = 1)

  Serial.print("0");
  setServoPulse(0, 1.0); // Move to 0° (1 ms pulse width)
  delay(1000);           // Wait for 1 second
  Serial.print("+");
  setServoPulse(0, 2.0); // Move to 180° (2 ms pulse width)
  delay(1000);           // Wait for 1 second

  // Turn LED off (set GPA7 low)
  mcp23017_write_GPIOA(0x00); // 0x00 is all bits low (GPA7 = 0)
  Serial.print("-");
  
  setServoPulse(0, 1.5); // Move back to neutral position (1.5 ms pulse width)
  delay(1000);           // Wait for 1 second
}
