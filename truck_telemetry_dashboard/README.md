# 🚛 Autonomous Truck Telemetry Dashboard

A real-time web dashboard for monitoring autonomous truck telemetry data via MQTT. Built with Flask and SocketIO for live updates.

![Dashboard Preview](https://img.shields.io/badge/Status-Real%20Time-brightgreen)
![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%205-red)
![Python](https://img.shields.io/badge/Python-3.8+-blue)

## Features

- 🔴 **Real-time telemetry display** - Live updates via WebSockets
- 📡 **MQTT Integration** - Connects to truck's MQTT broker
- 📊 **Visual sensor layout** - Intuitive ultrasonic sensor positioning
- 🎮 **Control monitoring** - Motor PWM and steering commands
- 🧭 **Navigation data** - Corridor centering and heading information
- 📈 **Telemetry history** - Recent message log with timestamps
- 📱 **Responsive design** - Works on mobile devices and tablets

## Installation

### On Raspberry Pi 5

1. **Clone/Copy the dashboard files:**
   ```bash
   cd /home/pi
   # Copy the truck_telemetry_dashboard directory here
   ```

2. **Create Python virtual environment:**
   ```bash
   cd truck_telemetry_dashboard
   python3 -m venv dashboard_env
   source dashboard_env/bin/activate
   ```

3. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

4. **Update MQTT broker IP (if needed):**
   ```bash
   nano app.py
   # Change MQTT_BROKER = "192.168.2.2" to your broker IP
   ```

### Easy Setup (Recommended)

Just use the provided launch script:
```bash
./start_dashboard.sh
```

## Development Mode

For layout development and testing without real trucks or MQTT infrastructure, use the development mode:

### 🔧 Development Dashboard Setup

The development mode provides:
- **Mock telemetry data** - Realistic sensor data generated locally  
- **No MQTT dependency** - Works without network infrastructure
- **Faster iteration** - Perfect for layout and UI development
- **Multiple truck simulation** - Tests multi-truck display

#### Quick Start (Development)
```bash
cd truck_telemetry_dashboard
./start_dev.sh
```

Or manually:
```bash
cd truck_telemetry_dashboard
python3 -m venv dashboard_env
source dashboard_env/bin/activate
pip install flask
python3 dashboard_dev.py
```

The development dashboard will be available at **http://localhost:5001**

#### Development vs Production
| Feature | Development | Production |
|---------|-------------|------------|
| **Data Source** | Mock/Random | Real MQTT |
| **Network** | None required | MQTT Broker |
| **Trucks** | 2 simulated | Real hardware |
| **Updates** | 1Hz fixed | 1Hz throttled |
| **Use Case** | Layout/UI work | Live monitoring |
cd truck_telemetry_dashboard
chmod +x start_dashboard.sh
./start_dashboard.sh
```

The script will automatically:
- Create virtual environment if needed
- Install dependencies
- Check MQTT broker connectivity  
- Start the dashboard

## Usage

### Start the Dashboard

```bash
cd truck_telemetry_dashboard
python3 app.py
```

### Access the Dashboard

- **Local access:** http://localhost:5000
- **Network access:** http://[PI_IP_ADDRESS]:5000
- **From any device on network:** http://192.168.2.X:5000

### Auto-start on Boot (Raspberry Pi)

Create a systemd service:

```bash
sudo nano /etc/systemd/system/truck-dashboard.service
```

Add this content:
```ini
[Unit]
Description=Truck Telemetry Dashboard
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/truck_telemetry_dashboard
ExecStart=/usr/bin/python3 app.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable truck-dashboard.service
sudo systemctl start truck-dashboard.service
```

## Configuration

### MQTT Settings (in app.py)

```python
MQTT_BROKER = "192.168.2.2"    # Your MQTT broker IP
MQTT_PORT = 1883               # MQTT broker port
MQTT_TOPIC = "telemetry"       # Topic truck publishes to
```

### Web Server Settings

```python
host='0.0.0.0'    # Listen on all interfaces
port=5000         # Web server port
debug=False       # Set True for development
```

## Telemetry Data Format

The dashboard expects JSON telemetry in this format:

```json
{
  "truck_id": "b4328a0a8ab4",
  "seq": 123,
  "t_ms": 45670,
  "mode": "EXPLORE", 
  "ul": 45.2,         // Ultrasonic left (cm)
  "ur": 38.7,         // Ultrasonic right (cm)  
  "uf": 67.1,         // Ultrasonic front (cm)
  "ub": 42.3,         // Ultrasonic back (cm)
  "yaw_rate": 1.23,   // Gyro yaw rate (deg/s)
  "heading": 0.0,     // Compass heading (deg)
  "width": 83.9,      // Corridor width (cm)
  "center_error": -6.5, // Center error (cm)
  "front_blocked": false,
  "cmd_pwm": 70,      // Motor PWM command
  "cmd_steer": "LEFT" // Steering command
}
```

## API Endpoints

- `GET /` - Main dashboard page
- `GET /api/status` - System status JSON
- `GET /api/telemetry` - Latest telemetry JSON
- `GET /api/history` - Telemetry history JSON

## Troubleshooting

### MQTT Connection Issues

1. **Check broker IP:** Ensure MQTT_BROKER matches your setup
2. **Network connectivity:** `ping 192.168.2.2` from Pi
3. **Firewall:** Ensure port 1883 is accessible
4. **Broker status:** Check MQTT broker is running

### Web Access Issues

1. **Port binding:** Ensure port 5000 is not in use
2. **Firewall:** Check Pi firewall allows port 5000
3. **Network:** Ensure Pi is on same network as accessing device

### Service Logs

```bash
sudo journalctl -u truck-dashboard.service -f
```

## Development

### Local Development

```bash
# Install dependencies
pip3 install -r requirements.txt

# Run with debug enabled
python3 app.py
```

Set `debug=True` in app.py for development mode with auto-reload.

### File Structure

```
truck_telemetry_dashboard/
├── app.py              # Main Flask application
├── requirements.txt    # Python dependencies  
├── README.md          # This file
├── templates/
│   └── index.html     # Dashboard HTML template
└── static/
    └── style.css      # Dashboard styling
```

## System Requirements

- **Python 3.8+**
- **Raspberry Pi 5** (or any Linux system)
- **Network access** to MQTT broker
- **Web browser** supporting WebSockets

## License

MIT License - See LICENSE file for details.

## Support

For issues or questions:
1. Check the troubleshooting section
2. Verify MQTT broker connectivity
3. Check system logs for errors