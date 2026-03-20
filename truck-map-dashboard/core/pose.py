"""
Pose estimation for the truck mapping system.
"""

import math
import time
from dataclasses import dataclass
from typing import Optional
from core.telemetry import TelemetryPacket


@dataclass
class Pose:
    """Represents the truck's pose in 2D space."""
    x: float        # X position in meters
    y: float        # Y position in meters  
    theta: float    # Heading in radians
    timestamp: float
    
    def get_heading_degrees(self) -> float:
        """Get heading in degrees (0-360)."""
        degrees = math.degrees(self.theta) % 360
        return degrees
    
    def copy(self) -> 'Pose':
        """Create a copy of this pose."""
        return Pose(self.x, self.y, self.theta, self.timestamp)


class PoseEstimator:
    """Estimates truck pose from telemetry data."""
    
    def __init__(self, config):
        self.config = config
        
        # Initialize pose
        self.current_pose = Pose(
            x=config.INITIAL_X,
            y=config.INITIAL_Y, 
            theta=math.radians(config.INITIAL_HEADING),
            timestamp=time.time()
        )
        
        self.last_update_time = None
        self.last_packet = None
        
    def update(self, packet: TelemetryPacket) -> Pose:
        """
        Update pose estimate with new telemetry packet.
        
        Args:
            packet: New telemetry data
            
        Returns:
            Updated pose
        """
        current_time = packet.received_at
        
        if self.last_update_time is not None and self.last_packet is not None:
            dt = current_time - self.last_update_time
            
            # Ensure reasonable time delta (0.01s to 2.0s)
            if 0.01 <= dt <= 2.0:
                self._update_pose(packet, dt)
        
        # Update heading from compass/heading sensor
        # Convert from degrees to radians, with 0° = east
        heading_rad = math.radians(packet.heading % 360)
        self.current_pose.theta = heading_rad
        self.current_pose.timestamp = current_time
        
        self.last_update_time = current_time
        self.last_packet = packet
        
        return self.current_pose.copy()
    
    def _update_pose(self, packet: TelemetryPacket, dt: float):
        """Update position based on motion model."""
        
        # Calculate speed from PWM command
        base_speed = packet.cmd_pwm * self.config.SPEED_PER_PWM
        base_speed = min(base_speed, self.config.MAX_SPEED_MPS)
        
        # Apply speed modifications based on mode and steering
        speed = self._apply_speed_modifiers(base_speed, packet)
        
        # Calculate distance traveled
        distance = speed * dt
        
        # Use previous heading for motion (current position update)
        # This assumes the heading change happens after the motion
        dx = distance * math.cos(self.current_pose.theta)
        dy = distance * math.sin(self.current_pose.theta)
        
        # Update position
        self.current_pose.x += dx
        self.current_pose.y += dy
    
    def _apply_speed_modifiers(self, base_speed: float, packet: TelemetryPacket) -> float:
        """Apply speed modifications based on truck state."""
        speed = base_speed
        
        # Reduce speed if turning
        if packet.cmd_steer != "STRAIGHT":
            speed *= self.config.TURN_SPEED_FACTOR
            
        # Reduce speed if in recovery mode
        if packet.mode == "RECOVER":
            speed *= self.config.RECOVERY_SPEED_FACTOR
            
        # Consider front_blocked if available
        if hasattr(packet, 'front_blocked') and packet.front_blocked:
            speed *= 0.1  # Nearly stop if blocked
            
        return max(0.0, speed)  # Ensure non-negative
    
    def reset(self, x: float = None, y: float = None, heading_deg: float = None):
        """Reset pose to given position and heading."""
        if x is None:
            x = self.config.INITIAL_X
        if y is None:
            y = self.config.INITIAL_Y
        if heading_deg is None:
            heading_deg = self.config.INITIAL_HEADING
            
        self.current_pose = Pose(
            x=x,
            y=y,
            theta=math.radians(heading_deg % 360),
            timestamp=time.time()
        )
        self.last_update_time = None
        self.last_packet = None
    
    def get_pose(self) -> Pose:
        """Get current pose estimate."""
        return self.current_pose.copy()
    
    def get_sensor_positions(self) -> dict:
        """
        Get world positions of all sensors based on current pose.
        
        Returns:
            dict: sensor_name -> (world_x, world_y, world_angle)
        """
        positions = {}
        
        for sensor_name, (offset_x_cm, offset_y_cm) in self.config.SENSOR_OFFSETS.items():
            # Convert cm to meters
            offset_x_m = offset_x_cm / 100.0
            offset_y_m = offset_y_cm / 100.0
            
            # Rotate offset by truck heading
            cos_theta = math.cos(self.current_pose.theta)
            sin_theta = math.sin(self.current_pose.theta)
            
            rotated_x = offset_x_m * cos_theta - offset_y_m * sin_theta  
            rotated_y = offset_x_m * sin_theta + offset_y_m * cos_theta
            
            # World position
            world_x = self.current_pose.x + rotated_x
            world_y = self.current_pose.y + rotated_y
            
            # Sensor angle (truck heading + sensor relative angle)
            sensor_rel_angle = math.radians(self.config.SENSOR_ANGLES[sensor_name])
            world_angle = self.current_pose.theta + sensor_rel_angle
            
            positions[sensor_name] = (world_x, world_y, world_angle)
            
        return positions