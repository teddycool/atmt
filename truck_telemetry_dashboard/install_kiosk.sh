#!/bin/bash

# Truck Telemetry Dashboard Kiosk Mode Installation Script
# For Raspberry Pi OS Bookworm Lite (headless)
# This script installs minimal GUI environment and sets up the dashboard service

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_NAME="truck-dashboard"
APP_USER="pi"
APP_DIR="$SCRIPT_DIR"

echo "=== Truck Telemetry Dashboard Kiosk Mode Installation ==="
echo "This script will:"
echo "1. Install minimal GUI environment (X11 + Openbox)"
echo "2. Install Chromium browser"
echo "3. Install Python dependencies"
echo "4. Create systemd service for the dashboard"
echo "5. Create autostart for kiosk mode browser"
echo ""

read -p "Do you want to continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled."
    exit 0
fi

echo ""
echo "=== Step 1: Updating package manager ==="
sudo apt update

echo ""
echo "=== Step 2: Installing minimal GUI environment ==="
# Install minimal X11 and window manager
sudo apt install -y \
    xorg \
    openbox \
    lightdm \
    chromium-browser \
    unclutter \
    x11-xserver-utils

echo ""
echo "=== Step 3: Installing Python dependencies ==="
# Install Python pip if not already installed
sudo apt install -y python3-pip python3-venv

# Create virtual environment for the app
python3 -m venv "$APP_DIR/venv"
source "$APP_DIR/venv/bin/activate"

# Install Python requirements
pip install -r "$APP_DIR/requirements.txt"

echo ""
echo "=== Step 4: Creating systemd service ==="
# Create service file
sudo tee /etc/systemd/system/${SERVICE_NAME}.service > /dev/null << EOF
[Unit]
Description=Truck Telemetry Dashboard
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=3
User=${APP_USER}
WorkingDirectory=${APP_DIR}
Environment=PATH=${APP_DIR}/venv/bin
ExecStart=${APP_DIR}/venv/bin/python ${APP_DIR}/app.py
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo ""
echo "=== Step 5: Setting up auto-login and kiosk mode ==="
# Configure LightDM for auto-login
sudo tee /etc/lightdm/lightdm.conf > /dev/null << EOF
[Seat:*]
autologin-user=${APP_USER}
autologin-user-timeout=0
user-session=openbox
EOF

# Create openbox autostart directory
mkdir -p /home/${APP_USER}/.config/openbox

# Create autostart script for kiosk mode
tee /home/${APP_USER}/.config/openbox/autostart > /dev/null << 'EOF'
#!/bin/bash

# Wait for dashboard service to be ready
sleep 10

# Disable screen blanking and power management
xset s off
xset -dpms
xset s noblank

# Hide cursor
unclutter -idle 1 &

# Start Chromium in kiosk mode
chromium-browser \
  --kiosk \
  --no-sandbox \
  --disable-dev-shm-usage \
  --disable-gpu \
  --disable-features=VizDisplayCompositor \
  --start-fullscreen \
  --app=http://localhost:5001 &
EOF

# Make autostart script executable
chmod +x /home/${APP_USER}/.config/openbox/autostart

# Set ownership of config files
sudo chown -R ${APP_USER}:${APP_USER} /home/${APP_USER}/.config

echo ""
echo "=== Step 6: Creating helper scripts ==="
# Create start script
tee "$APP_DIR/start_kiosk.sh" > /dev/null << EOF
#!/bin/bash
# Start the dashboard service and enable GUI
sudo systemctl start ${SERVICE_NAME}
sudo systemctl start lightdm
EOF
chmod +x "$APP_DIR/start_kiosk.sh"

# Create stop script
tee "$APP_DIR/stop_kiosk.sh" > /dev/null << EOF
#!/bin/bash
# Stop kiosk mode and dashboard service
sudo systemctl stop lightdm
sudo systemctl stop ${SERVICE_NAME}
EOF
chmod +x "$APP_DIR/stop_kiosk.sh"

# Create status script
tee "$APP_DIR/status.sh" > /dev/null << EOF
#!/bin/bash
echo "=== Dashboard Service Status ==="
sudo systemctl status ${SERVICE_NAME} --no-pager -l
echo ""
echo "=== GUI Service Status ==="
sudo systemctl status lightdm --no-pager -l
echo ""
echo "=== Dashboard Logs (last 20 lines) ==="
sudo journalctl -u ${SERVICE_NAME} -n 20 --no-pager
EOF
chmod +x "$APP_DIR/status.sh"

echo ""
echo "=== Step 7: Enabling services ==="
# Reload systemd and enable services
sudo systemctl daemon-reload
sudo systemctl enable ${SERVICE_NAME}
sudo systemctl enable lightdm

# Configure boot to GUI mode
sudo systemctl set-default graphical.target

echo ""
echo "=== Installation Complete! ==="
echo ""
echo "Next steps:"
echo "1. Reboot your Raspberry Pi: sudo reboot"
echo "2. The system will automatically:"
echo "   - Start the dashboard service"
echo "   - Login to desktop automatically"
echo "   - Open Chromium in kiosk mode showing the dashboard"
echo ""
echo "Useful commands:"
echo "- Check status: ./status.sh"
echo "- Start kiosk manually: ./start_kiosk.sh"
echo "- Stop kiosk: ./stop_kiosk.sh"
echo "- View logs: sudo journalctl -u ${SERVICE_NAME} -f"
echo "- Restart dashboard: sudo systemctl restart ${SERVICE_NAME}"
echo ""
echo "To access the dashboard remotely:"
echo "- Open http://$(hostname -I | awk '{print $1}'):5001 in any browser"
echo ""
echo "To disable kiosk mode and return to headless:"
echo "- sudo systemctl set-default multi-user.target"
echo "- sudo systemctl disable lightdm"
echo "- sudo reboot"

read -p "Do you want to reboot now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Rebooting in 3 seconds..."
    sleep 3
    sudo reboot
else
    echo "Please reboot manually when ready: sudo reboot"
fi