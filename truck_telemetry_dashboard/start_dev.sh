#!/bin/bash
# Development Dashboard Startup Script
echo "🚛 Starting Autonomous Truck Dashboard - Development Mode"
echo "📡 This uses mock telemetry data for layout testing"
echo ""

# Check if we're in the dashboard directory
if [ ! -f "dashboard_dev.py" ]; then
    echo "❌ Please run this script from the truck_telemetry_dashboard directory"
    exit 1
fi

# Check if virtual environment exists
if [ ! -d "dashboard_env" ]; then
    echo "🔨 Creating Python virtual environment..."
    python3 -m venv dashboard_env
fi

echo "🔌 Activating virtual environment..."
source dashboard_env/bin/activate

echo "📦 Installing/updating dependencies..."
pip install --quiet flask

echo "🚀 Starting development dashboard server..."
echo "🌐 Dashboard will be available at: http://localhost:5001"
echo "🔄 Mock telemetry updates every second"
echo "💡 Press Ctrl+C to stop the server"
echo ""

python3 dashboard_dev.py