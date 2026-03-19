# Arduino OTA (Over-The-Air) Update Process for ATMT Trucks

## Overview

The ATMT project uses Arduino OTA to wirelessly update firmware on ESP32-based trucks without requiring physical access to the devices. This document describes the current implementation and how to perform OTA updates.

## How OTA Works in ATMT

### 1. OTA Initialization

The OTA functionality is implemented in the main control loop (`z_main_control_loop.cpp`) and follows these steps:

1. **WiFi Connection**: Each truck connects to the configured WiFi network using credentials from `secrets.h`
2. **Hostname Setup**: Each truck gets a unique hostname: `truck-{chipId}` where chipId is derived from the ESP32's hardware
3. **OTA Configuration**: Arduino OTA is configured with authentication and callback handlers
4. **OTA Ready**: The system becomes ready to receive firmware updates

### 2. OTA Configuration Details

```cpp
// Hostname: truck-{unique_chip_id}
ArduinoOTA.setHostname(("truck-" + chipId).c_str());

// Fixed password for all trucks
ArduinoOTA.setPassword("truck123");
```

### 3. OTA Event Handlers

The system includes comprehensive error handling and progress monitoring:

- **onStart()**: Logs when update begins (sketch or filesystem)
- **onEnd()**: Confirms successful completion
- **onProgress()**: Shows percentage progress during upload
- **onError()**: Handles various error conditions:
  - Authentication failures
  - Connection issues
  - Upload errors
  - Begin/End errors

### 4. Runtime Operation

In the main control loop, `ArduinoOTA.handle()` is called continuously to:
- Listen for incoming OTA requests
- Process firmware uploads
- Handle authentication
- Manage the update process

## Performing an OTA Update

### Prerequisites

1. **Network Access**: Ensure you're on the same WiFi network as the trucks
2. **Arduino IDE**: Install Arduino IDE with ESP32 board support
3. **Source Code**: Have the updated firmware ready to compile

### Update Process

1. **Identify Target Truck**:
   - Each truck broadcasts its hostname: `truck-{chipId}`
   - Use network discovery tools or check serial output for the exact hostname

2. **Configure Arduino IDE**:
   - Open your project in Arduino IDE
   - Go to `Tools > Port`
   - Select the network port showing your target truck (e.g., `truck-abc123 at 192.168.1.100`)

3. **Upload Firmware**:
   - Click "Upload" or press `Ctrl+U`
   - Enter the OTA password: `truck123`
   - Monitor progress in the IDE console

4. **Verify Update**:
   - Check serial output for "OTA Update Complete!"
   - Verify the truck reboots and functions correctly
   - Confirm new functionality is working

### Alternative: Command Line Upload

You can also use `arduino-cli` or `platformio` for OTA uploads:

```bash
# Using platformio (if configured)
pio run -e control_loop --target upload --upload-port truck-{chipId}.local

# The system will prompt for the OTA password: truck123
```

## Network Requirements

### WiFi Configuration

- **SSID & Password**: Configured in `secrets.h`
- **Network Type**: 2.4GHz WiFi (ESP32 limitation)
- **Connectivity**: Trucks must maintain stable connection during update

### Firewall Considerations

- **Port 3232**: Arduino OTA default port (must be open)
- **mDNS**: Multicast DNS for hostname resolution
- **Local Network**: OTA only works on the same subnet

## Security Considerations

### Current Implementation

- **Fixed Password**: All trucks use the same OTA password (`truck123`)
- **Network Security**: Relies on WiFi network security
- **Plain Text**: Password is hardcoded in source code

### Recommendations

For production deployment, consider:

1. **Unique Passwords**: Generate unique OTA passwords per device
2. **Certificate-based Auth**: Implement certificate-based authentication
3. **Encrypted Updates**: Add firmware encryption
4. **Access Control**: Implement role-based access control

## Troubleshooting

### Common Issues

1. **Truck Not Found**:
   - Verify truck is connected to WiFi
   - Check hostname in serial output
   - Ensure same network subnet

2. **Authentication Failed**:
   - Verify password is `truck123`
   - Check for typos in password entry

3. **Upload Timeout**:
   - Ensure stable WiFi connection
   - Try again during low network activity
   - Check truck isn't in a control loop that blocks OTA handling

4. **Update Failed**:
   - Check available flash memory
   - Verify firmware size is within limits
   - Monitor serial output for specific error codes

### Debugging

Enable verbose output in Arduino IDE:
- `File > Preferences > Show verbose output during: upload`
- Check serial monitor for detailed error messages

## Code Structure

### Key Files

- **Main Loop**: `EmbeddedSw/src/z_main_control_loop.cpp`
  - OTA initialization (lines ~605-634)
  - OTA handling in main loop (line ~737)

- **Includes**: Required libraries
  - `#include <ArduinoOTA.h>`
  - `#include <WiFi.h>`

- **Dependencies**: PlatformIO automatically includes Arduino OTA library

### Integration Points

OTA is integrated at two main points:

1. **Setup Phase**: Initialize OTA after WiFi connection
2. **Main Loop**: Call `ArduinoOTA.handle()` regularly

This ensures OTA updates can be received and processed while the truck continues normal operation.

## Best Practices

1. **Stable WiFi**: Ensure strong WiFi signal before starting updates
2. **Backup Firmware**: Keep working firmware versions for rollback
3. **Test Updates**: Validate new firmware on a single truck first
4. **Scheduled Updates**: Perform updates during maintenance windows
5. **Monitor Progress**: Watch serial output and IDE progress during updates
6. **Version Control**: Use clear version numbering for firmware builds

## Limitations

- **Network Dependency**: Requires stable WiFi connection
- **Single Update**: One truck can be updated at a time per IDE instance
- **Flash Constraints**: Update size limited by available flash memory
- **Timing Sensitive**: Main loop must frequently call `ArduinoOTA.handle()`