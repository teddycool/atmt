#!/bin/bash

# Truck Telemetry Dashboard - Manual Service Management
# Use this if you want to manage the service without the full kiosk installation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_NAME="truck-dashboard"
APP_USER="pi"

case "$1" in
    install)
        echo "Installing dashboard service..."
        
        # Install Python dependencies
        echo "Installing Python dependencies..."
        sudo apt update
        sudo apt install -y python3-pip python3-venv
        
        # Create virtual environment
        python3 -m venv "$SCRIPT_DIR/venv"
        source "$SCRIPT_DIR/venv/bin/activate"
        pip install -r "$SCRIPT_DIR/requirements.txt"
        
        # Install service file
        sudo cp "$SCRIPT_DIR/truck-dashboard.service" "/etc/systemd/system/"
        
        # Update service file with correct paths
        sudo sed -i "s|/home/pi/truck_telemetry_dashboard|$SCRIPT_DIR|g" "/etc/systemd/system/$SERVICE_NAME.service"
        sudo sed -i "s|User=pi|User=$USER|g" "/etc/systemd/system/$SERVICE_NAME.service"
        
        # Enable service
        sudo systemctl daemon-reload
        sudo systemctl enable $SERVICE_NAME
        
        echo "Service installed successfully!"
        echo "Use: sudo systemctl start $SERVICE_NAME"
        ;;
        
    uninstall)
        echo "Uninstalling dashboard service..."
        sudo systemctl stop $SERVICE_NAME 2>/dev/null || true
        sudo systemctl disable $SERVICE_NAME 2>/dev/null || true
        sudo rm -f "/etc/systemd/system/$SERVICE_NAME.service"
        sudo systemctl daemon-reload
        echo "Service uninstalled."
        ;;
        
    status)
        sudo systemctl status $SERVICE_NAME --no-pager
        ;;
        
    logs)
        sudo journalctl -u $SERVICE_NAME -f
        ;;
        
    start)
        sudo systemctl start $SERVICE_NAME
        echo "Dashboard started. Access at: http://$(hostname -I | awk '{print $1}'):5001"
        ;;
        
    stop)
        sudo systemctl stop $SERVICE_NAME
        ;;
        
    restart)
        sudo systemctl restart $SERVICE_NAME
        echo "Dashboard restarted. Access at: http://$(hostname -I | awk '{print $1}'):5001"
        ;;
        
    *)
        echo "Usage: $0 {install|uninstall|start|stop|restart|status|logs}"
        echo ""
        echo "  install   - Install service and dependencies"
        echo "  uninstall - Remove service"
        echo "  start     - Start the dashboard"
        echo "  stop      - Stop the dashboard"
        echo "  restart   - Restart the dashboard"
        echo "  status    - Show service status"
        echo "  logs      - Show live logs"
        exit 1
        ;;
esac