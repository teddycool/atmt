Create a professional technical architecture diagram in landscape A3 format showing an autonomous truck racing telemetry system. The diagram should include:

Top Section - Physical Truck:

Realistic autonomous truck with visible sensors: 4 ultrasonic sensors (front, back, left, right), compass/magnetometer, accelerometer/gyroscope
ESP32 microcontroller board clearly labeled inside truck
WiFi signal waves emanating from truck to network
Steering servo and motor drive system illustrated
All sensors connected to ESP32 with labeled data flows
Center Section - Network Infrastructure:

MQTT Broker server (192.168.2.2:1883) as central hub
Bidirectional data flow arrows showing:
Truck publishing telemetry every 1Hz
Topics: chipId/telemetry, chipId/control
JSON payload examples floating near arrows
Bottom Left - Real-time Dashboard:

Computer/laptop showing Flask web interface (port 5001)
Live charts displaying: sensor readings, truck position, speed, heading
WebSocket connection to MQTT broker
Server-side throttling mechanism illustrated
Dashboard showing 1Hz update rate for smooth UX

Bottom Right - ML Backend Service (Future):

Server/cloud icon labeled "Track Mapping & Lap Time Optimization"
Machine learning algorithms processing:
Historical telemetry data
Track geometry mapping
Racing line optimization
Lap time prediction models
Feedback loop to truck for optimized racing parameters
Dashed lines indicating "Not Yet Implemented"
Additional Elements:

Title: "Autonomous Racing Truck - Telemetry & Optimization Architecture"
Data flow directions with labeled protocols (MQTT, WebSocket, HTTP)
Technical specifications in corner: ESP32, PlatformIO, Python Flask, MQTT 1024-byte buffer
Color coding: Green for implemented, Blue for data flow, Orange for future ML components
Professional engineering diagram style with clean lines and technical fonts
Grid background suitable for technical documentation
The diagram should be clear enough for engineers to understand the system flow and identify areas where they can contribute to project completion.

Image created • Autonomous racing truck telemetry and design
Generated image: Autonomous racing truck telemetry and design

Edited image
great but replace the truck at the top with this image

Image created • Autonomous racing truck telemetry architecture
Generated image: Autonomous racing truck telemetry architecture

Change the headline to Autonomous Toy truck - Telemetry & Architecture 

Image created • Autonomous toy truck telemetry architecture
Generated image: Autonomous toy truck telemetry architecture


