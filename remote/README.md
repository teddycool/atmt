# Remote Control and Computer Vision

This directory contains remote control capabilities for the autonomous trucks, including manual control scripts and computer vision-based steering.

## Components

### 1. Manual Remote Control (`remote.py`, `remote.sh`)

Scripts for manual control of the trucks via MQTT commands.

**Usage:**
```bash
python3 remote.py
# or
./remote.sh
```

Control with keyboard:
- **W**: Move forward
- **A**: Turn left
- **D**: Turn right
- **S**: Move backward/stop

### 2. Computer Vision Detection (`remote_image_detection.py`)

YOLO-based object detection system that can automatically steer the truck based on visual input.

**Setup:**
1. Create a virtual environment:
   ```bash
   python3 -m venv yolo_env
   source yolo_env/bin/activate  # Linux/Mac
   # or
   yolo_env\Scripts\activate     # Windows
   ```

2. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

3. Run the detection system:
   ```bash
   python3 remote_image_detection.py
   ```

**Features:**
- Object detection using YOLOv8n model
- Real-time steering commands via MQTT
- Configurable for different truck ESP32 IDs

### 3. Bash Remote Control (`bash_remote_control/`)

Alternative bash-based remote control scripts:
- `detection_script.py`: Computer vision detection
- `remote_json.sh`: JSON-based MQTT commands
- `remote.sh`: Simple control interface

## Configuration

Make sure to update the ESP32 chip ID in the scripts to match your truck:
- Check the serial output from your ESP32 for the chip ID
- Update the topic name in the scripts (format: `{chipId}/control`)

## MQTT Topics

The remote control uses these MQTT topics:
- **Control commands**: `{chipId}/control`
- **Telemetry data**: `{chipId}/sensors`

## Network Requirements

- Same WiFi network as the trucks
- MQTT broker accessible at configured IP (usually 192.168.2.2)
- Python 3.8+ for computer vision features
