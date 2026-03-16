#!/bin/bash

# Truck Telemetry Dashboard Launcher
# Simple script to start the dashboard with proper error handling and virtual environment

echo "🚛 Starting Autonomous Truck Telemetry Dashboard..."
echo "=============================================="

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: python3 is not installed or not in PATH"
    exit 1
fi

# Check if virtual environment exists, if not create it
if [ ! -d "dashboard_env" ]; then
    echo "📦 Creating Python virtual environment..."
    python3 -m venv dashboard_env
    echo "✅ Virtual environment created"
fi

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source dashboard_env/bin/activate

# Check if required packages are installed in venv
dashboard_env/bin/python -c "
import flask
import flask_socketio
import paho.mqtt.client
print('All required packages found')
" 2>/dev/null

if [ $? -ne 0 ]; then
    echo "⚠️  Warning: Required packages not installed in virtual environment. Installing now..."
    dashboard_env/bin/pip install --upgrade pip
    dashboard_env/bin/pip install -r requirements.txt
    
    # Verify installation
    dashboard_env/bin/python -c "
import flask
import flask_socketio  
import paho.mqtt.client
print('✅ All packages installed successfully')
" 2>/dev/null
    
    if [ $? -ne 0 ]; then
        echo "❌ Error: Package installation failed"
        exit 1
    fi
fi

# Check if MQTT broker is reachable
echo "🔍 Checking MQTT broker connectivity..."
timeout 3 bash -c 'cat < /dev/null > /dev/tcp/192.168.2.2/1883' 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ MQTT broker at 192.168.2.2:1883 is reachable"
else
    echo "⚠️  Warning: Cannot connect to MQTT broker at 192.168.2.2:1883"
    echo "   Dashboard will start but won't receive data until broker is available"
fi

echo ""
echo "🌐 Dashboard will be available at:"
echo "   Local:   http://localhost:5000"
echo "   Network: http://$(hostname -I | awk '{print $1}'):5000"
echo ""
echo "📊 Watching for telemetry on topic: telemetry"
echo "🔄 Press Ctrl+C to stop"
echo ""

# Start the Flask application
dashboard_env/bin/python app.py