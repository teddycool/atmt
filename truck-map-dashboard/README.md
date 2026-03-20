# Truck Mapping Dashboard

A real-time web-based dashboard for mapping and tracking an autonomous toy truck. This is **Step 1** of a mapping system that receives telemetry from a truck and creates a live 2D occupancy grid map.

## Features

- **Real-time telemetry processing**: Receives truck sensor data via HTTP POST
- **Live pose estimation**: Tracks truck position and heading using motion model
- **Occupancy grid mapping**: Creates 2D maps using ultrasonic sensor data
- **Web dashboard**: Live-updating browser interface with WebSocket
- **SQLite logging**: Stores all telemetry data for analysis
- **Test mode**: Includes telemetry simulator for testing without real hardware

## Tech Stack

- **Python 3.11+**
- **Flask** - Web framework
- **Flask-SocketIO** - Real-time communication
- **NumPy** - Numerical computation for mapping
- **Pillow** - Image rendering
- **SQLite** - Data storage
- **eventlet** - Async support

## Project Structure

```
truck-map-dashboard/
├── app.py                 # Main Flask application
├── config.py             # Configuration settings
├── requirements.txt      # Python dependencies
├── telemetry_sim.py      # Test data generator
├── README.md             # This file
├── templates/
│   └── index.html        # Web dashboard UI
├── static/
│   └── style.css         # Dashboard styling
└── core/                 # Core mapping modules
    ├── telemetry.py      # Telemetry data handling
    ├── pose.py           # Pose estimation
    ├── occupancy_grid.py # Occupancy grid mapping
    ├── renderer.py       # Map visualization
    └── database.py       # SQLite operations
```

## Quick Start

### 1. Setup Python Environment

In VS Code, open a terminal and create a virtual environment:

```bash
# Navigate to project directory
cd truck-map-dashboard

# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate  # On Linux/Mac
# or
venv\Scripts\activate     # On Windows
```

### 2. Install Dependencies

```bash
pip install -r requirements.txt
```

### 3. Run the Dashboard

```bash
python app.py
```

The dashboard will start at: **http://localhost:5000**

### 4. Test with Simulator

In a new terminal (with the virtual environment activated):

```bash
python telemetry_sim.py
```

This will generate fake truck telemetry and you should see the map updating in real-time!

## Usage

### Dashboard Interface

The web dashboard shows:

- **Live Map**: Real-time occupancy grid with truck position and heading
- **Truck Status**: Current position, heading, mode, and control commands  
- **Sensor Readings**: Ultrasonic sensor values (front, back, left, right)
- **System Stats**: Packet counts, map statistics, error counts
- **Controls**: Reset map, export PNG, view debug info

### API Endpoints

- `POST /api/telemetry` - Receive telemetry data
- `GET /api/status` - Get system status
- `GET /api/map/image` - Get current map as base64 PNG
- `POST /api/map/reset` - Reset map to empty state
- `POST /api/map/export` - Export map as PNG file
- `GET /api/debug` - Get detailed debug information

### Telemetry Format

Send JSON data to `/api/telemetry` with this structure:

```json
{
  "truck_id": "8a0a8ab4",
  "seq": 3282,
  "t_ms": 1876933,
  "free_heap": 216128,
  "mode": "RECOVER",
  "ul": 199.0,
  "ur": 199.0, 
  "uf": 18.0,
  "ub": 53.2,
  "yaw_rate": 1.88,
  "gy_x": -22.95,
  "gy_y": 23.25,
  "gy_z": 246.69,
  "heading": 144.3,
  "compass": 144.3,
  "mag_x": 10.00,
  "mag_y": 11.00,
  "mag_z": -36.02,
  "acc_x": -143.58,
  "acc_y": 4390.02,
  "acc_z": 15981.24,
  "width": 398.0,
  "center_error": -0.0,
  "front_blocked": true,
  "cmd_pwm": 95,
  "cmd_steer": "LEFT"
}
```

## Configuration

Edit `config.py` to tune the mapping system:

### Motion Model
```python
SPEED_PER_PWM = 0.005       # Speed in m/s per PWM unit
MAX_SPEED_MPS = 0.5         # Maximum speed 
TURN_SPEED_FACTOR = 0.3     # Speed reduction while turning
```

### Map Settings
```python
MAP_WIDTH_M = 10.0          # Map width in meters
MAP_HEIGHT_M = 10.0         # Map height in meters  
CELL_SIZE_M = 0.05          # Resolution: 5cm per cell
```

### Sensor Model
```python
SENSOR_MAX_RANGE_CM = 180.0 # Maximum trusted range
SENSOR_MIN_RANGE_CM = 2.0   # Minimum trusted range
SENSOR_INVALID_VALUE = 199.0 # Value for "no return"
```

## How It Works

### 1. Telemetry Processing
- Validates incoming JSON packets
- Parses sensor readings and control commands
- Stores raw data in SQLite database

### 2. Pose Estimation
- Uses heading from compass/IMU as primary orientation
- Estimates forward motion from PWM commands and time
- Applies speed reductions for turning and recovery modes
- Maintains (x, y, θ) pose estimate

### 3. Occupancy Grid Mapping
- Creates 2D grid with configurable resolution (default: 5cm cells)
- Uses log-odds representation for occupancy probabilities
- For each ultrasonic sensor:
  - Traces ray from sensor to detected obstacle
  - Marks free space along ray
  - Marks endpoint as occupied (if valid reading)
- Handles sensor placement and orientation relative to truck

### 4. Visualization
- Renders occupancy grid as grayscale image
- Overlays truck position (red circle) and heading (red line)
- Shows coordinate system and scale information
- Updates in real-time via WebSocket

## Limitations (Step 1)

This is a basic mapping system with these limitations:

- **No SLAM**: No loop closure or scan matching
- **Simple motion model**: Basic PWM-to-speed conversion
- **No sensor fusion**: Relies primarily on heading sensor
- **No obstacle tracking**: Static occupancy representation
- **Limited sensor model**: Simple ray-casting model

## Next Steps

Future improvements could include:

1. **Scan matching** for improved pose accuracy
2. **Loop closure detection** for map consistency  
3. **Kalman filtering** for sensor fusion
4. **Dynamic obstacle tracking**
5. **Path planning integration**
6. **Camera data processing** with Hailo acceleration

## Troubleshooting

### Dashboard won't load
- Check if Flask app is running: `python app.py`
- Verify port 5000 is not in use
- Check browser console for JavaScript errors

### Simulator not connecting
- Ensure Flask app is running first
- Check firewall/network settings
- Verify URL in telemetry_sim.py matches Flask server

### Map not updating
- Check WebSocket connection status in browser
- Verify telemetry data format matches expected JSON
- Check browser developer tools for errors
- Look at Flask console output for error messages

### Tuning the motion model
- Adjust `SPEED_PER_PWM` in config.py based on your truck's speed
- Modify `TURN_SPEED_FACTOR` if turning behavior looks wrong
- Change `SENSOR_MAX_RANGE_CM` to match your sensor specifications

### Performance issues
- Reduce map size or increase cell size for better performance
- Lower telemetry update rate if CPU usage is high
- Check available RAM for large maps

## Development

### Adding new features
1. Core mapping logic goes in the `core/` modules
2. Web API endpoints are added to `app.py`
3. UI updates go in `templates/index.html` and `static/style.css`
4. Configuration options are defined in `config.py`

### Testing
- Use `telemetry_sim.py` for development testing
- Modify simulator parameters to test edge cases
- Export maps as PNG for visual verification
- Check debug endpoint for detailed state information

---

**Happy Mapping! 🗺️🚛**