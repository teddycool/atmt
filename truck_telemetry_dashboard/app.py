#!/usr/bin/env python3
"""
Autonomous Truck Telemetry Dashboard
Flask web application with real-time MQTT telemetry display
"""

from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
import json
import threading
import time
from datetime import datetime
import signal
import sys

app = Flask(__name__)
app.config['SECRET_KEY'] = 'truck_telemetry_secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# MQTT Configuration
MQTT_BROKER = "192.168.2.2"
MQTT_PORT = 1883
MQTT_TOPIC = "+/telemetry"
MQTT_KEEPALIVE = 60

# Global variables for telemetry data
latest_telemetry_by_truck = {}  # Dictionary keyed by truck_id
telemetry_history_by_truck = {}  # Dictionary of lists keyed by truck_id
last_dashboard_emission_by_truck = {}  # Track last WebSocket emission time per truck
MAX_HISTORY = 100  # Keep last 100 telemetry messages per truck
DASHBOARD_UPDATE_INTERVAL = 1.0  # Dashboard updates once per second
is_connected = False
last_update_time = None

# MQTT Client Setup
try:
    # Try new API format first (paho-mqtt >= 2.0)
    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
except:
    # Fall back to old API format (paho-mqtt < 2.0)
    try:
        mqtt_client = mqtt.Client(client_id="", clean_session=True, userdata=None, protocol=mqtt.MQTTv311, transport="tcp")
    except:
        # Even older format
        mqtt_client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    """Callback when MQTT client connects"""
    global is_connected
    if rc == 0:
        print(f"✅ Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
        print(f"📡 Subscribing to topic: {MQTT_TOPIC}")
        client.subscribe(MQTT_TOPIC)
        is_connected = True
        socketio.emit('connection_status', {'connected': True})
    else:
        print(f"❌ Failed to connect to MQTT broker. Return code {rc}")
        is_connected = False

def on_disconnect(client, userdata, rc):
    """Callback when MQTT client disconnects"""
    global is_connected
    print(f"🔌 Disconnected from MQTT broker. Return code: {rc}")
    is_connected = False
    socketio.emit('connection_status', {'connected': False})

def sanitize_telemetry_data(data):
    """
    Sanitize telemetry data to handle NaN values and other edge cases.
    NaN values are converted to None which JSON serializes as null.
    """
    import math
    
    def sanitize_value(value):
        if value is None:
            return None
        if isinstance(value, float):
            if math.isnan(value):
                return None
            elif math.isinf(value):
                return None
        return value
    
    sanitized = {}
    nan_keys = []
    
    for key, value in data.items():
        original_value = value
        sanitized_value = sanitize_value(value)
        sanitized[key] = sanitized_value
        
        # Track which keys had NaN values for logging
        if original_value != sanitized_value and isinstance(original_value, float):
            nan_keys.append(key)
    
    # Log if any NaN values were found
    if nan_keys:
        truck_id = data.get('truck_id', 'unknown')
        print(f"🔧 Sanitized NaN values in truck {truck_id}: {nan_keys}")
    
    return sanitized

def on_message(client, userdata, msg):
    """Callback when MQTT message is received"""
    global latest_telemetry_by_truck, telemetry_history_by_truck, last_update_time, last_dashboard_emission_by_truck
    
    # Debug: Print all received messages
    print(f"🔔 MQTT Message received on topic: '{msg.topic}'")
    print(f"📝 Raw payload: {msg.payload}")
    
    try:
        # Parse JSON telemetry data
        payload = msg.payload.decode('utf-8')
        print(f"📄 Decoded payload: {payload}")
        
        telemetry_data = json.loads(payload)
        
        # Sanitize NaN and infinity values before processing
        telemetry_data = sanitize_telemetry_data(telemetry_data)
        
        # Get truck_id, use 'unknown' if not provided
        truck_id = telemetry_data.get('truck_id', 'unknown')
        
        # Add timestamp
        telemetry_data['received_at'] = datetime.now().strftime('%H:%M:%S')
        telemetry_data['received_timestamp'] = time.time()
        
        # ALWAYS update latest telemetry for this truck (for mapping/logging)
        latest_telemetry_by_truck[truck_id] = telemetry_data
        last_update_time = time.time()
        
        # Initialize history for new trucks
        if truck_id not in telemetry_history_by_truck:
            telemetry_history_by_truck[truck_id] = []
        
        # ALWAYS add to history (preserves all data for mapping)
        telemetry_history_by_truck[truck_id].append(telemetry_data)
        if len(telemetry_history_by_truck[truck_id]) > MAX_HISTORY:
            telemetry_history_by_truck[truck_id].pop(0)
        
        # THROTTLED dashboard updates: Only emit to WebSocket once per second per truck
        current_time = time.time()
        last_emission = last_dashboard_emission_by_truck.get(truck_id, 0)
        
        if (current_time - last_emission) >= DASHBOARD_UPDATE_INTERVAL:
            # Time to update the dashboard for this truck
            last_dashboard_emission_by_truck[truck_id] = current_time
            
            # Emit throttled update to web clients via WebSocket
            socketio.emit('telemetry_update', {
                'truck_id': truck_id,
                'data': telemetry_data,
                'all_trucks': list(latest_telemetry_by_truck.keys())
            })
            
            print(f"📊 Dashboard updated for {telemetry_data.get('truck_id', 'unknown')}: "
                  f"Mode: {telemetry_data.get('mode', 'N/A')}, "
                  f"Seq: {telemetry_data.get('seq', 'N/A')} [THROTTLED]")
        else:
            # Skip dashboard update but data is still stored
            skip_time = DASHBOARD_UPDATE_INTERVAL - (current_time - last_emission)
            print(f"⏭️  Dashboard update skipped for {truck_id} (next in {skip_time:.1f}s) - "
                  f"Data preserved for mapping: Seq {telemetry_data.get('seq', 'N/A')}")
        
    except json.JSONDecodeError as e:
        print(f"❌ Error parsing telemetry JSON: {e}")
        print(f"   Raw payload was: {msg.payload}")
    except Exception as e:
        print(f"❌ Error processing telemetry: {e}")
        print(f"   Topic: {msg.topic}, Payload: {msg.payload}")

# Set MQTT callbacks
mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('index.html')

@app.route('/api/status')
def api_status():
    """API endpoint for system status"""
    current_time = time.time()
    data_age = None
    if last_update_time:
        data_age = current_time - last_update_time
    
    total_history_count = sum(len(history) for history in telemetry_history_by_truck.values())
    
    return jsonify({
        'mqtt_connected': is_connected,
        'trucks': list(latest_telemetry_by_truck.keys()),
        'latest_telemetry_by_truck': latest_telemetry_by_truck,
        'total_history_count': total_history_count,
        'data_age_seconds': data_age,
        'server_time': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    })

@app.route('/api/telemetry/full')
def api_telemetry_full():
    """API endpoint for full-rate telemetry data (for mapping/logging)"""
    return jsonify({
        'trucks': list(latest_telemetry_by_truck.keys()),
        'telemetry_by_truck': latest_telemetry_by_truck,
        'note': 'This endpoint provides full-rate telemetry data for mapping purposes'
    })

@app.route('/api/telemetry')
def api_telemetry():
    """API endpoint for latest telemetry data (dashboard-throttled)"""
    return jsonify({
        'trucks': list(latest_telemetry_by_truck.keys()),
        'telemetry_by_truck': latest_telemetry_by_truck,
        'throttled': True,
        'update_interval': DASHBOARD_UPDATE_INTERVAL
    })

@app.route('/api/debug')
def api_debug():
    """Debug endpoint to verify throttling configuration"""
    return jsonify({
        'throttling_enabled': True,
        'dashboard_update_interval': DASHBOARD_UPDATE_INTERVAL,
        'last_emissions': {k: v for k, v in last_dashboard_emission_by_truck.items()},
        'current_time': time.time(),
        'version': 'throttled_v2',
        'total_trucks': len(latest_telemetry_by_truck),
        'message': 'If you see this, the server has throttling code'
    })

@app.route('/api/history')
def api_history():
    """API endpoint for telemetry history"""
    history_response = {}
    for truck_id, history in telemetry_history_by_truck.items():
        history_response[truck_id] = {
            'history': history[-20:],  # Last 20 entries per truck
            'count': len(history)
        }
    
    return jsonify({
        'history_by_truck': history_response,
        'total_trucks': len(telemetry_history_by_truck)
    })

@socketio.on('connect')
def handle_connect():
    """Handle WebSocket client connection"""
    print(f"🔗 WebSocket client connected")
    emit('connection_status', {'connected': is_connected})
    
    # Send current telemetry data to newly connected client (respecting throttling)
    if latest_telemetry_by_truck:
        current_time = time.time()
        for truck_id, telemetry_data in latest_telemetry_by_truck.items():
            # Always send data to new clients, but update emission time for throttling
            last_dashboard_emission_by_truck[truck_id] = current_time
            
            emit('telemetry_update', {
                'truck_id': truck_id,
                'data': telemetry_data,
                'all_trucks': list(latest_telemetry_by_truck.keys())
            })
            print(f"📤 Initial telemetry sent to new client for truck: {truck_id}")

@socketio.on('disconnect')
def handle_disconnect():
    """Handle WebSocket client disconnection"""
    print(f"🔗 WebSocket client disconnected")

@socketio.on('request_status')
def handle_status_request():
    """Handle status request from client"""
    current_time = time.time()
    data_age = None
    if last_update_time:
        data_age = current_time - last_update_time
    
    total_history_count = sum(len(history) for history in telemetry_history_by_truck.values())
    
    emit('status_update', {
        'mqtt_connected': is_connected,
        'data_age_seconds': data_age,
        'history_count': total_history_count
    })

def mqtt_thread():
    """MQTT client thread"""
    print(f"🚀 Starting MQTT client thread...")
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"❌ MQTT thread error: {e}")

def signal_handler(sig, frame):
    """Handle graceful shutdown"""
    print('\n🛑 Shutting down gracefully...')
    mqtt_client.disconnect()
    sys.exit(0)

if __name__ == '__main__':
    print("""
    🚛 Autonomous Truck Telemetry Dashboard
    =======================================
    MQTT Broker: {}:{}
    Topic: {}
    
    Starting web server...
    """.format(MQTT_BROKER, MQTT_PORT, MQTT_TOPIC))
    
    # Handle Ctrl+C gracefully
    signal.signal(signal.SIGINT, signal_handler)
    
    # Start MQTT client in background thread
    mqtt_thread = threading.Thread(target=mqtt_thread, daemon=True)
    mqtt_thread.start()
    
    # Start Flask-SocketIO server
    try:
        socketio.run(app, 
                    host='0.0.0.0',  # Listen on all interfaces for Pi access
                    port=5001, 
                    debug=False,  # Set to True for development
                    allow_unsafe_werkzeug=True)
    except KeyboardInterrupt:
        signal_handler(None, None)