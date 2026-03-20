#!/bin/bash

# Kiosk Mode Browser Autostart Script
# This script runs automatically when the GUI starts

# Wait for dashboard service to be ready
echo "Waiting for dashboard service to start..."
sleep 10

# Disable screen blanking and power management
echo "Configuring display settings..."
xset s off          # Disable screen saver
xset -dpms          # Disable power management
xset s noblank      # Disable screen blanking

# Hide mouse cursor after 1 second of inactivity
echo "Starting cursor hide utility..."
unclutter -idle 1 &

# Wait a bit more to ensure the dashboard is fully loaded
sleep 5

# Start Chromium in kiosk mode
echo "Starting Chromium in kiosk mode..."
chromium-browser \
  --kiosk \
  --no-sandbox \
  --disable-dev-shm-usage \
  --disable-gpu \
  --disable-features=VizDisplayCompositor \
  --disable-infobars \
  --disable-notifications \
  --disable-session-crashed-bubble \
  --disable-component-extensions-with-background-pages \
  --start-fullscreen \
  --app=http://localhost:5001 &

# Keep script running
wait