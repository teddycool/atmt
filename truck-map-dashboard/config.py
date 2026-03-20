# Truck Map Dashboard Configuration

class Config:
    """Main configuration class for the truck mapping system."""
    
    # Flask Configuration
    SECRET_KEY = 'truck-mapper-secret-key-2026'
    DEBUG = True
    
    # Database Configuration
    DATABASE_PATH = 'telemetry.db'
    
    # Map Configuration
    MAP_WIDTH_M = 10.0          # Map width in meters
    MAP_HEIGHT_M = 10.0         # Map height in meters
    CELL_SIZE_M = 0.05          # Cell resolution in meters per cell
    MAP_CENTER_X = 0.0          # Map center X coordinate
    MAP_CENTER_Y = 0.0          # Map center Y coordinate
    
    # Occupancy Grid Parameters
    LOG_ODDS_FREE = -0.4        # Log-odds decrease for free space
    LOG_ODDS_OCCUPIED = 0.9     # Log-odds increase for occupied space
    LOG_ODDS_CLAMP_MIN = -5.0   # Minimum log-odds value
    LOG_ODDS_CLAMP_MAX = 5.0    # Maximum log-odds value
    
    # Sensor Configuration
    SENSOR_MAX_RANGE_CM = 180.0     # Maximum trusted sensor range in cm
    SENSOR_MIN_RANGE_CM = 2.0       # Minimum trusted sensor range in cm
    SENSOR_INVALID_VALUE = 199.0    # Value indicating invalid/max range reading
    
    # Sensor positions relative to truck center (in cm)
    SENSOR_OFFSETS = {
        'uf': (15.0, 0.0),      # Front sensor: 15cm forward
        'ub': (-15.0, 0.0),     # Back sensor: 15cm backward  
        'ul': (0.0, 10.0),      # Left sensor: 10cm left
        'ur': (0.0, -10.0)      # Right sensor: 10cm right
    }
    
    # Sensor beam directions relative to truck heading (in degrees)
    SENSOR_ANGLES = {
        'uf': 0.0,              # Front: straight ahead
        'ub': 180.0,            # Back: straight behind
        'ul': 90.0,             # Left: 90 degrees left
        'ur': -90.0             # Right: 90 degrees right
    }
    
    # Motion Model Configuration
    SPEED_PER_PWM = 0.005       # Speed in m/s per PWM unit (tune this!)
    MAX_SPEED_MPS = 0.5         # Maximum speed in m/s
    TURN_SPEED_FACTOR = 0.3     # Speed reduction factor while turning
    RECOVERY_SPEED_FACTOR = 0.1 # Speed reduction factor in recovery mode
    
    # Pose Estimation
    INITIAL_X = 0.0             # Starting X position
    INITIAL_Y = 0.0             # Starting Y position  
    INITIAL_HEADING = 0.0       # Starting heading in degrees (0 = east)
    
    # Rendering Configuration
    MAP_IMAGE_WIDTH = 600       # Output image width in pixels
    MAP_IMAGE_HEIGHT = 600      # Output image height in pixels
    TRUCK_MARKER_SIZE = 8       # Size of truck marker in pixels
    
    # WebSocket Configuration
    SOCKETIO_ASYNC_MODE = 'eventlet'
    
    # MQTT Configuration
    MQTT_BROKER_HOST = '192.168.2.2'
    MQTT_BROKER_PORT = 1883
    MQTT_TOPIC = '8a0a8ab4/telemetry'
    MQTT_KEEPALIVE = 60
    MQTT_USERNAME = None  # No authentication needed
    MQTT_PASSWORD = None
    
    # Telemetry Validation
    REQUIRED_FIELDS = [
        'truck_id', 'seq', 't_ms', 'mode', 
        'ul', 'ur', 'uf', 'ub', 
        'heading', 'cmd_pwm', 'cmd_steer'
    ]