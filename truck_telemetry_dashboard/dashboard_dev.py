#!/usr/bin/env python3
"""
Dashboard Development Server with Mock Telemetry Data
Simplified Flask app for testing dashboard layout without MQTT/WebSocket complexity
"""

from flask import Flask, render_template, jsonify
import threading
import time
import random
import math
from datetime import datetime

app = Flask(__name__)

# Mock telemetry data storage
mock_telemetry_data = {}
mock_active_trucks = ["truck-abc123", "truck-def456"]  # Simulate 2 trucks

def generate_mock_telemetry(truck_id, seq):
    """Generate realistic mock telemetry data matching the ESP32 format"""
    
    # Simulate different truck states
    modes = ["IDLE", "EXPLORE", "RECOVER", "STOP"]
    steers = ["STRAIGHT", "LEFT", "RIGHT"]
    
    # Generate realistic sensor readings (in cm)
    base_width = random.uniform(35, 55)  # Track width variation
    center_offset = random.uniform(-8, 8)  # How centered the truck is
    
    ul = base_width/2 + center_offset + random.uniform(-2, 2)  # left sensor
    ur = base_width/2 - center_offset + random.uniform(-2, 2)  # right sensor
    uf = random.uniform(8, 40)  # front sensor (obstacles)
    ub = random.uniform(15, 50)  # back sensor
    
    # Clamp sensors to realistic ranges
    ul = max(2, min(200, ul))
    ur = max(2, min(200, ur))
    uf = max(2, min(200, uf))
    ub = max(2, min(200, ub))
    
    # Derived features
    width = ul + ur
    center_error = ur - ul
    front_blocked = uf < 15
    
    # IMU data
    heading = (time.time() * 10) % 360  # Slowly rotating
    compass = heading + random.uniform(-5, 5)
    yaw_rate = random.uniform(-10, 10)
    
    # Accelerometer (simulate movement)
    acc_x = random.uniform(-1, 1)
    acc_y = random.uniform(-1, 1)
    acc_z = 9.8 + random.uniform(-0.5, 0.5)
    
    # Magnetometer
    mag_x = random.uniform(-50, 50)
    mag_y = random.uniform(-50, 50)
    mag_z = random.uniform(-50, 50)
    
    # Commands
    cmd_pwm = random.choice([0, 45, 70, -30])  # Idle, slow, explore, reverse
    cmd_steer = random.choice(steers)
    mode = random.choice(modes)
    
    # Match exact ESP32 JSON format
    telemetry = {
        "truck_id": truck_id,
        "seq": seq,
        "t_ms": int(time.time() * 1000),
        "mode": mode,
        "ul": round(ul, 1),
        "ur": round(ur, 1), 
        "uf": round(uf, 1),
        "ub": round(ub, 1),
        "yaw_rate": round(yaw_rate, 2),
        "heading": round(heading, 1),
        "compass": round(compass, 1),
        "mag_x": round(mag_x, 1),
        "mag_y": round(mag_y, 1),
        "mag_z": round(mag_z, 1),
        "acc_x": round(acc_x, 1),
        "acc_y": round(acc_y, 1),
        "acc_z": round(acc_z, 1),
        "width": round(width, 1),
        "center_error": round(center_error, 1),
        "front_blocked": front_blocked,
        "cmd_pwm": cmd_pwm,
        "cmd_steer": cmd_steer,
        # Dashboard additions
        "received_at": datetime.now().strftime('%H:%M:%S'),
        "received_timestamp": time.time()
    }
    
    return telemetry

def mock_telemetry_generator():
    """Background thread generating mock telemetry data"""
    seq_counters = {truck_id: 0 for truck_id in mock_active_trucks}
    
    while True:
        for truck_id in mock_active_trucks:
            seq_counters[truck_id] += 1
            telemetry = generate_mock_telemetry(truck_id, seq_counters[truck_id])
            mock_telemetry_data[truck_id] = telemetry
            print(f"🔄 Generated mock telemetry for {truck_id}: seq={seq_counters[truck_id]}, mode={telemetry['mode']}")
        
        time.sleep(1)  # 1Hz update rate

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('index_dev.html')

@app.route('/api/trucks')
def api_trucks():
    """API endpoint to get current truck data"""
    return jsonify({
        'trucks': mock_telemetry_data,
        'active_trucks': mock_active_trucks,
        'connected': True,
        'last_update': time.time()
    })

@app.route('/api/status')
def api_status():
    """API endpoint for system status"""
    return jsonify({
        'mqtt_connected': True,  # Always true for mock
        'data_age_seconds': 0,   # Always fresh for mock
        'active_trucks': len(mock_active_trucks),
        'message': 'Mock telemetry running'
    })

if __name__ == '__main__':
    # Start mock telemetry generator in background
    telemetry_thread = threading.Thread(target=mock_telemetry_generator, daemon=True)
    telemetry_thread.start()
    
    print("🚛 Starting Dashboard Development Server with Mock Telemetry")
    print("📡 Simulating trucks:", mock_active_trucks)
    print("🌐 Dashboard available at: http://localhost:5002")
    print("🔄 Mock telemetry updating at 1Hz")
    print("💡 Use this for layout development without real trucks/MQTT")
    
    app.run(debug=True, host='0.0.0.0', port=5002)