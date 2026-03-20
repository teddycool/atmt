# Truck Telemetry Dashboard - Kiosk Mode Setup

This directory contains scripts to set up your Flask dashboard to run in kiosk mode on a Raspberry Pi 3B+ with Bookworm Lite.

## Quick Setup

1. **Copy this entire folder to your Raspberry Pi** (e.g., to `/home/pi/truck_telemetry_dashboard/`)

2. **Run the installation script:**
   ```bash
   chmod +x install_kiosk.sh
   sudo ./install_kiosk.sh
   ```

3. **Reboot when prompted:**
   ```bash
   sudo reboot
   ```

The system will automatically start in kiosk mode showing your dashboard!

## What the Installation Does

### Installs Minimal GUI Environment:
- **Xorg** - X11 window system
- **Openbox** - Lightweight window manager
- **LightDM** - Display manager for auto-login
- **Chromium** - Browser for kiosk mode
- **Unclutter** - Hides mouse cursor

### Creates System Service:
- **truck-dashboard.service** - Auto-starts your Flask app
- Runs as the `pi` user
- Auto-restarts on failure
- Starts after network is available

### Configures Kiosk Mode:
- Auto-login to desktop
- Automatically opens Chromium in fullscreen
- Disables screen saver and power management
- Hides mouse cursor
- Points browser to `http://localhost:5001`

## Manual Service Management

If you only want the service without the GUI (headless mode):

```bash
chmod +x service_manager.sh

# Install service only
./service_manager.sh install

# Control the service
./service_manager.sh start
./service_manager.sh stop
./service_manager.sh restart
./service_manager.sh status
./service_manager.sh logs
```

## Helper Scripts

After installation, these scripts are available:

- **`./status.sh`** - Check service and GUI status
- **`./start_kiosk.sh`** - Manually start kiosk mode
- **`./stop_kiosk.sh`** - Stop kiosk mode and return to terminal
- **`./service_manager.sh`** - Manage just the service

## Remote Access

Your dashboard will be accessible from other devices on the network:
```
http://[PI_IP_ADDRESS]:5001
```

## Troubleshooting

### Check Service Status:
```bash
sudo systemctl status truck-dashboard
```

### View Logs:
```bash
sudo journalctl -u truck-dashboard -f
```

### Check if GUI is Running:
```bash
sudo systemctl status lightdm
```

### Return to Headless Mode:
```bash
sudo systemctl set-default multi-user.target
sudo systemctl disable lightdm
sudo reboot
```

### Return to Kiosk Mode:
```bash
sudo systemctl set-default graphical.target
sudo systemctl enable lightdm
sudo reboot
```

## Configuration Notes

### Network Configuration
Make sure your MQTT broker IP address is correct in `app.py`:
```python
MQTT_BROKER = "192.168.2.2"  # Update this to match your network
```

### Display Settings
If you need to adjust the display resolution or orientation, you can modify `/boot/config.txt`:
```
# For specific resolution
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=82  # 1920x1080 60Hz

# For screen rotation
display_rotate=1  # 90 degrees
```

### Browser Settings
To modify the browser behavior, edit the autostart script that gets created during installation.

## System Requirements

- Raspberry Pi 3B+ or newer
- Raspberry Pi OS Bookworm Lite (headless)
- At least 2GB SD card space for GUI components
- Network connection for MQTT communication