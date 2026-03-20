"""
Main Flask application for the truck mapping dashboard.
"""

import os
import io
import base64
import json
import time
from flask import Flask, request, jsonify, render_template
from flask_socketio import SocketIO, emit
import eventlet

# Import our core modules
from config import Config
from core.telemetry import TelemetryProcessor
from core.pose import PoseEstimator  
from core.occupancy_grid import OccupancyMapper
from core.renderer import MapRenderer
from core.database import DatabaseManager
from core.mqtt_client import MQTTTelemetryClient

# Force eventlet monkey patching
eventlet.monkey_patch()

# Create Flask app
app = Flask(__name__)
app.config.from_object(Config)

# Initialize SocketIO
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# Global state
telemetry_processor = None
pose_estimator = None
occupancy_mapper = None
map_renderer = None
database_manager = None
mqtt_client = None
app_stats = {
    'start_time': time.time(),
    'last_update': None,
    'error_count': 0,
    'http_packets': 0,
    'mqtt_packets': 0
}


def init_components():
    """Initialize all system components."""
    global telemetry_processor, pose_estimator, occupancy_mapper, map_renderer, database_manager, mqtt_client
    
    print("Initializing truck mapping system...")
    
    # Initialize components
    telemetry_processor = TelemetryProcessor(Config)
    pose_estimator = PoseEstimator(Config)
    occupancy_mapper = OccupancyMapper(Config, pose_estimator)
    map_renderer = MapRenderer(Config)
    
    # Initialize database
    db_path = Config.DATABASE_PATH
    database_manager = DatabaseManager(db_path)
    
    # Initialize MQTT client
    mqtt_client = MQTTTelemetryClient(Config, process_mqtt_telemetry)
    
    print("System initialized successfully!")


def process_mqtt_telemetry(json_data: str):
    """Process telemetry data received from MQTT."""
    try:
        # Use the same processing logic as HTTP endpoint
        success, packet, error_msg = telemetry_processor.process_json(json_data)
        
        if not success:
            app_stats['error_count'] += 1
            print(f"MQTT telemetry error: {error_msg}")
            return
        
        # Update pose estimation
        pose = pose_estimator.update(packet)
        
        # Update occupancy map
        occupancy_mapper.update(packet, pose)
        
        # Store in database
        database_manager.insert_telemetry(packet)
        database_manager.insert_pose(packet.truck_id, packet.seq, pose, pose.timestamp)
        
        # Update stats
        app_stats['last_update'] = time.time()
        app_stats['mqtt_packets'] += 1
        
        # Emit real-time update via WebSocket
        emit_realtime_update(packet, pose)
        
        print(f"Processed MQTT packet {packet.seq} from {packet.truck_id}")
        
    except Exception as e:
        app_stats['error_count'] += 1
        print(f"Error processing MQTT telemetry: {e}")


@app.route('/')
def index():
    """Main dashboard page."""
    return render_template('index.html')


@app.route('/api/telemetry', methods=['POST'])
def receive_telemetry():
    """
    Endpoint for receiving telemetry data from the truck.
    
    Expected JSON format:
    {
      "truck_id": "8a0a8ab4",
      "seq": 3282,
      "t_ms": 1876933,
      ...
    }
    """
    try:
        # Get JSON data
        json_data = request.get_data(as_text=True)
        
        if not json_data:
            return jsonify({'error': 'No data received'}), 400
        
        # Process telemetry
        success, packet, error_msg = telemetry_processor.process_json(json_data)
        
        if not success:
            app_stats['error_count'] += 1
            return jsonify({'error': error_msg}), 400
        
        # Update pose estimation
        pose = pose_estimator.update(packet)
        
        # Update occupancy map
        occupancy_mapper.update(packet, pose)
        
        # Store in database
        database_manager.insert_telemetry(packet)
        database_manager.insert_pose(packet.truck_id, packet.seq, pose, pose.timestamp)
        
        # Update stats
        app_stats['last_update'] = time.time()
        app_stats['http_packets'] += 1
        
        # Emit real-time update via WebSocket
        emit_realtime_update(packet, pose)
        
        return jsonify({'status': 'success', 'seq': packet.seq})
        
    except Exception as e:
        app_stats['error_count'] += 1
        print(f"Error processing telemetry: {e}")
        return jsonify({'error': f'Internal server error: {str(e)}'}), 500


@app.route('/api/status')
def get_status():
    """Get current system status."""
    try:
        # Get current pose
        current_pose = pose_estimator.get_pose()
        
        # Get system stats
        telemetry_stats = telemetry_processor.get_stats()
        map_stats = occupancy_mapper.get_stats()
        db_stats = database_manager.get_telemetry_stats()
        
        uptime = time.time() - app_stats['start_time']
        
        # Get MQTT status
        mqtt_status = mqtt_client.get_status() if mqtt_client else None
        
        status = {
            'uptime_seconds': uptime,
            'last_update': app_stats['last_update'],
            'error_count': app_stats['error_count'],
            'http_packets': app_stats['http_packets'],
            'mqtt_packets': app_stats['mqtt_packets'],
            'mqtt': mqtt_status,
            'pose': {
                'x': current_pose.x,
                'y': current_pose.y,
                'heading_deg': current_pose.get_heading_degrees(),
                'timestamp': current_pose.timestamp
            },
            'telemetry': telemetry_stats,
            'mapping': map_stats,
            'database': db_stats
        }
        
        return jsonify(status)
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/map/image')
def get_map_image():
    """Get current map as a PNG image (base64 encoded)."""
    try:
        # Get current state
        current_pose = pose_estimator.get_pose()
        grid = occupancy_mapper.get_grid()
        
        # Render map
        img = map_renderer.render_map(grid, current_pose, show_truck=True)
        
        # Convert to base64
        img_buffer = io.BytesIO()
        img.save(img_buffer, format='PNG')
        img_data = base64.b64encode(img_buffer.getvalue()).decode()
        
        return jsonify({
            'image_data': img_data,
            'content_type': 'image/png',
            'timestamp': time.time()
        })
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/map/reset', methods=['POST'])
def reset_map():
    """Reset the occupancy map to empty state."""
    try:
        occupancy_mapper.reset()
        pose_estimator.reset()
        
        return jsonify({'status': 'success', 'message': 'Map reset successfully'})
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/map/export', methods=['POST'])
def export_map():
    """Export current map as PNG file."""
    try:
        filename = request.json.get('filename', f'truck_map_{int(time.time())}.png')
        
        # Get current state
        current_pose = pose_estimator.get_pose()
        grid = occupancy_mapper.get_grid()
        
        # Save map
        map_renderer.save_map_png(grid, current_pose, filename)
        
        return jsonify({'status': 'success', 'filename': filename})
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/mqtt/status')
def get_mqtt_status():
    """Get MQTT connection status."""
    try:
        if mqtt_client:
            return jsonify(mqtt_client.get_status())
        else:
            return jsonify({'error': 'MQTT client not initialized'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/mqtt/reconnect', methods=['POST'])
def reconnect_mqtt():
    """Reconnect to MQTT broker."""
    try:
        if mqtt_client:
            mqtt_client.stop()
            success = mqtt_client.start()
            if success:
                return jsonify({'status': 'success', 'message': 'MQTT reconnection initiated'})
            else:
                return jsonify({'status': 'error', 'message': 'Failed to reconnect to MQTT broker'})
        else:
            return jsonify({'error': 'MQTT client not initialized'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/debug')
def get_debug_info():
    """Get detailed debug information."""
    try:
        current_pose = pose_estimator.get_pose()
        grid = occupancy_mapper.get_grid()
        
        # Get sensor positions
        sensor_positions = pose_estimator.get_sensor_positions()
        
        # Get last telemetry packet
        last_packet = telemetry_processor.last_packet
        packet_dict = None
        if last_packet:
            packet_dict = {
                'truck_id': last_packet.truck_id,
                'seq': last_packet.seq,
                'mode': last_packet.mode,
                'ul': last_packet.ul,
                'ur': last_packet.ur,
                'uf': last_packet.uf,
                'ub': last_packet.ub,
                'heading': last_packet.heading,
                'cmd_pwm': last_packet.cmd_pwm,
                'cmd_steer': last_packet.cmd_steer,
                'received_at': last_packet.received_at
            }
        
        debug_info = {
            'pose': {
                'x': current_pose.x,
                'y': current_pose.y,
                'theta_rad': current_pose.theta,
                'heading_deg': current_pose.get_heading_degrees(),
                'timestamp': current_pose.timestamp
            },
            'grid_info': {
                'width_cells': grid.width_cells,
                'height_cells': grid.height_cells,
                'cell_size_m': Config.CELL_SIZE_M,
                'origin_x': grid.origin_x,
                'origin_y': grid.origin_y
            },
            'sensor_positions': sensor_positions,
            'last_packet': packet_dict,
            'config': {
                'speed_per_pwm': Config.SPEED_PER_PWM,
                'max_speed_mps': Config.MAX_SPEED_MPS,
                'sensor_max_range_cm': Config.SENSOR_MAX_RANGE_CM,
                'map_size_m': (Config.MAP_WIDTH_M, Config.MAP_HEIGHT_M)
            }
        }
        
        return jsonify(debug_info)
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


def emit_realtime_update(packet, pose):
    """Emit real-time update to connected WebSocket clients."""
    try:
        # Prepare update data
        update_data = {
            'timestamp': time.time(),
            'truck_id': packet.truck_id,
            'seq': packet.seq,
            'mode': packet.mode,
            'pose': {
                'x': pose.x,
                'y': pose.y,
                'heading_deg': pose.get_heading_degrees()
            },
            'sensors': {
                'ul': packet.ul,
                'ur': packet.ur,
                'uf': packet.uf,
                'ub': packet.ub
            },
            'commands': {
                'pwm': packet.cmd_pwm,
                'steer': packet.cmd_steer
            }
        }
        
        # Emit to all connected clients
        socketio.emit('telemetry_update', update_data)
        
    except Exception as e:
        print(f"Error emitting WebSocket update: {e}")


@socketio.on('connect')
def handle_connect():
    """Handle WebSocket connection."""
    print('Client connected to WebSocket')
    
    # Send initial status
    try:
        current_pose = pose_estimator.get_pose()
        stats = {
            'telemetry': telemetry_processor.get_stats(),
            'mapping': occupancy_mapper.get_stats()
        }
        
        emit('initial_status', {
            'pose': {
                'x': current_pose.x,
                'y': current_pose.y,
                'heading_deg': current_pose.get_heading_degrees()
            },
            'stats': stats,
            'connected': True
        })
    except Exception as e:
        print(f"Error sending initial status: {e}")


@socketio.on('disconnect')
def handle_disconnect():
    """Handle WebSocket disconnection."""
    print('Client disconnected from WebSocket')


@socketio.on('request_map_update')
def handle_map_request():
    """Handle request for map image update."""
    try:
        # Get current state
        current_pose = pose_estimator.get_pose()
        grid = occupancy_mapper.get_grid()
        
        # Render map
        img = map_renderer.render_map(grid, current_pose, show_truck=True)
        
        # Convert to base64
        img_buffer = io.BytesIO()
        img.save(img_buffer, format='PNG')
        img_data = base64.b64encode(img_buffer.getvalue()).decode()
        
        # Emit map update
        emit('map_update', {
            'image_data': img_data,
            'timestamp': time.time()
        })
        
    except Exception as e:
        print(f"Error sending map update: {e}")


if __name__ == '__main__':
    # Initialize components
    init_components()
    
    # Start MQTT client
    print("Starting MQTT client...")
    mqtt_success = mqtt_client.start()
    if mqtt_success:
        print("MQTT client started successfully")
    else:
        print("Warning: MQTT client failed to start - HTTP telemetry still available")
    
    print("Starting truck mapping dashboard...")
    print(f"Dashboard will be available at: http://localhost:5000")
    print("Telemetry endpoint: http://localhost:5000/api/telemetry")
    print(f"MQTT broker: {Config.MQTT_BROKER_HOST}:{Config.MQTT_BROKER_PORT}")
    print(f"MQTT topic: {Config.MQTT_TOPIC}")
    
    try:
        # Run the app
        socketio.run(app, host='0.0.0.0', port=5000, debug=Config.DEBUG)
    finally:
        # Clean shutdown
        if mqtt_client:
            mqtt_client.stop()