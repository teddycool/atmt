"""
Telemetry data handling for the truck mapping system.
"""

from dataclasses import dataclass
from typing import Dict, Any, Optional
import json
import time


@dataclass
class TelemetryPacket:
    """Represents a single telemetry packet from the truck."""
    
    # Core identification
    truck_id: str
    seq: int
    t_ms: int
    
    # System status
    free_heap: Optional[int]
    mode: str
    
    # Ultrasonic sensors (in cm)
    ul: float  # Left
    ur: float  # Right  
    uf: float  # Front
    ub: float  # Back
    
    # Motion sensors
    yaw_rate: Optional[float]
    gy_x: Optional[float]
    gy_y: Optional[float]
    gy_z: Optional[float]
    heading: float
    compass: Optional[float]
    
    # Magnetometer
    mag_x: Optional[float]
    mag_y: Optional[float]
    mag_z: Optional[float]
    
    # Accelerometer
    acc_x: Optional[float]
    acc_y: Optional[float]
    acc_z: Optional[float]
    
    # Line following (if applicable)
    width: Optional[float]
    center_error: Optional[float]
    front_blocked: Optional[bool]
    
    # Control commands
    cmd_pwm: int
    cmd_steer: str
    
    # Timestamp when packet was received by server
    received_at: float = None
    
    def __post_init__(self):
        """Set received timestamp if not already set."""
        if self.received_at is None:
            self.received_at = time.time()


class TelemetryValidator:
    """Validates incoming telemetry data."""
    
    def __init__(self, required_fields: list):
        self.required_fields = required_fields
    
    def validate(self, data: Dict[str, Any]) -> tuple[bool, str]:
        """
        Validate telemetry data.
        
        Returns:
            tuple: (is_valid, error_message)
        """
        if not isinstance(data, dict):
            return False, "Data must be a dictionary"
        
        # Check required fields
        missing_fields = []
        for field in self.required_fields:
            if field not in data:
                missing_fields.append(field)
        
        if missing_fields:
            return False, f"Missing required fields: {', '.join(missing_fields)}"
        
        # Type validation
        try:
            # Numeric fields that should be numbers
            numeric_fields = ['seq', 't_ms', 'ul', 'ur', 'uf', 'ub', 'heading', 'cmd_pwm']
            for field in numeric_fields:
                if field in data and data[field] is not None:
                    float(data[field])  # Will raise ValueError if not numeric
            
            # String fields
            string_fields = ['truck_id', 'mode', 'cmd_steer']
            for field in string_fields:
                if field in data and data[field] is not None:
                    if not isinstance(data[field], str):
                        return False, f"Field {field} must be a string"
            
            return True, ""
            
        except (ValueError, TypeError) as e:
            return False, f"Type validation error: {str(e)}"
    
    def parse_packet(self, data: Dict[str, Any]) -> TelemetryPacket:
        """
        Parse validated data into a TelemetryPacket.
        
        Args:
            data: Validated telemetry data dictionary
            
        Returns:
            TelemetryPacket instance
        """
        # Helper function to safely get optional numeric values
        def get_optional_float(key: str) -> Optional[float]:
            val = data.get(key)
            return float(val) if val is not None else None
        
        def get_optional_int(key: str) -> Optional[int]:
            val = data.get(key)
            return int(val) if val is not None else None
        
        def get_optional_bool(key: str) -> Optional[bool]:
            val = data.get(key)
            return bool(val) if val is not None else None
        
        return TelemetryPacket(
            truck_id=data['truck_id'],
            seq=int(data['seq']),
            t_ms=int(data['t_ms']),
            free_heap=get_optional_int('free_heap'),
            mode=data['mode'],
            ul=float(data['ul']),
            ur=float(data['ur']),
            uf=float(data['uf']),
            ub=float(data['ub']),
            yaw_rate=get_optional_float('yaw_rate'),
            gy_x=get_optional_float('gy_x'),
            gy_y=get_optional_float('gy_y'),
            gy_z=get_optional_float('gy_z'),
            heading=float(data['heading']),
            compass=get_optional_float('compass'),
            mag_x=get_optional_float('mag_x'),
            mag_y=get_optional_float('mag_y'),
            mag_z=get_optional_float('mag_z'),
            acc_x=get_optional_float('acc_x'),
            acc_y=get_optional_float('acc_y'),
            acc_z=get_optional_float('acc_z'),
            width=get_optional_float('width'),
            center_error=get_optional_float('center_error'),
            front_blocked=get_optional_bool('front_blocked'),
            cmd_pwm=int(data['cmd_pwm']),
            cmd_steer=data['cmd_steer']
        )


class TelemetryProcessor:
    """Processes incoming telemetry packets."""
    
    def __init__(self, config):
        self.config = config
        self.validator = TelemetryValidator(config.REQUIRED_FIELDS)
        self.packet_count = 0
        self.last_packet = None
        
    def process_json(self, json_data: str) -> tuple[bool, TelemetryPacket, str]:
        """
        Process JSON telemetry data.
        
        Returns:
            tuple: (success, packet_or_none, error_message)
        """
        try:
            data = json.loads(json_data)
        except json.JSONDecodeError as e:
            return False, None, f"JSON decode error: {str(e)}"
        
        # Validate the data
        is_valid, error_msg = self.validator.validate(data)
        if not is_valid:
            return False, None, error_msg
        
        try:
            # Parse into packet
            packet = self.validator.parse_packet(data)
            self.packet_count += 1
            self.last_packet = packet
            return True, packet, ""
            
        except Exception as e:
            return False, None, f"Packet parsing error: {str(e)}"
    
    def get_stats(self) -> Dict[str, Any]:
        """Get processing statistics."""
        return {
            'packet_count': self.packet_count,
            'last_seq': self.last_packet.seq if self.last_packet else None,
            'last_truck_id': self.last_packet.truck_id if self.last_packet else None,
            'last_mode': self.last_packet.mode if self.last_packet else None,
            'last_received': self.last_packet.received_at if self.last_packet else None
        }