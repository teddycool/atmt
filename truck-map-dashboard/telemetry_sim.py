"""
Telemetry simulator for testing the truck mapping dashboard.

This script generates fake telemetry data and sends it to the Flask app
to test the mapping system without needing the real truck.
"""

import time
import json
import random
import math
import requests
import threading
from typing import Tuple


class TelemetrySimulator:
    """Simulates truck telemetry for testing purposes."""
    
    def __init__(self, server_url: str = "http://localhost:5000"):
        self.server_url = server_url
        self.endpoint = f"{server_url}/api/telemetry"
        
        # Simulation state
        self.truck_id = "SIM001"
        self.seq = 1
        self.t_ms = 0
        self.running = False
        
        # Simulated truck state
        self.x = 0.0          # Position in meters
        self.y = 0.0
        self.heading = 0.0    # Heading in degrees (0 = east)
        self.speed = 0.0      # Current speed
        
        # Simulation parameters
        self.update_rate = 10.0  # Hz
        self.arena_width = 8.0   # Arena width in meters
        self.arena_height = 6.0  # Arena height in meters
        
        # Obstacles (simple rectangular obstacles)
        self.obstacles = [
            (2.0, 2.0, 1.0, 1.0),  # x, y, width, height
            (5.0, 3.5, 1.5, 0.8),
            (1.0, 4.5, 0.8, 1.0),
        ]
        
        # Movement state machine
        self.mode = "EXPLORE"
        self.target_heading = 0.0
        self.last_turn_time = 0.0
        self.stuck_count = 0
        
        print(f"Telemetry Simulator initialized")
        print(f"Server URL: {self.server_url}")
        print(f"Arena size: {self.arena_width}m x {self.arena_height}m")
    
    def start(self):
        """Start the telemetry simulation."""
        if self.running:
            print("Simulator already running!")
            return
            
        self.running = True
        self.t_ms = 0
        
        print("Starting telemetry simulator...")
        print("Press Ctrl+C to stop")
        
        try:
            self._simulation_loop()
        except KeyboardInterrupt:
            self.stop()
    
    def stop(self):
        """Stop the telemetry simulation."""
        self.running = False
        print("\nSimulator stopped.")
    
    def _simulation_loop(self):
        """Main simulation loop."""
        dt = 1.0 / self.update_rate
        
        while self.running:
            start_time = time.time()
            
            # Update simulation state
            self._update_truck_behavior(dt)
            self._update_position(dt)
            
            # Generate telemetry packet
            packet = self._generate_telemetry_packet()
            
            # Send to server
            self._send_telemetry(packet)
            
            # Update counters
            self.seq += 1
            self.t_ms += int(dt * 1000)
            
            # Sleep to maintain update rate
            elapsed = time.time() - start_time
            sleep_time = max(0, dt - elapsed)
            time.sleep(sleep_time)
    
    def _update_truck_behavior(self, dt: float):
        """Update truck behavior and movement decisions."""
        current_time = time.time()
        
        # Get sensor readings for decision making
        sensors = self._get_sensor_readings()
        front_dist = sensors['uf']
        left_dist = sensors['ul']
        right_dist = sensors['ur']
        back_dist = sensors['ub']
        
        # Simple behavior state machine
        if self.mode == "EXPLORE":
            # Check if we need to avoid obstacles
            if front_dist < 30.0:  # Front obstacle detected
                self.mode = "AVOID"
                self.stuck_count += 1
                
                # Choose turn direction based on available space
                if left_dist > right_dist:
                    self.target_heading = (self.heading + 90) % 360
                else:
                    self.target_heading = (self.heading - 90) % 360
                    
                self.last_turn_time = current_time
                
            elif self._is_near_boundary():
                # Turn away from boundaries
                self.mode = "TURN"
                center_x, center_y = self.arena_width / 2, self.arena_height / 2
                to_center_angle = math.degrees(math.atan2(center_y - self.y, center_x - self.x))
                self.target_heading = to_center_angle % 360
                self.last_turn_time = current_time
                
            else:
                # Continue exploring - occasionally change direction
                if random.random() < 0.02:  # 2% chance per update
                    self.target_heading = random.uniform(0, 360)
                    self.mode = "TURN"
                    self.last_turn_time = current_time
        
        elif self.mode == "AVOID" or self.mode == "TURN":
            # Check if turn is complete
            heading_diff = abs(self._angle_difference(self.heading, self.target_heading))
            
            if heading_diff < 15.0 or (current_time - self.last_turn_time) > 3.0:
                # Turn complete or timeout
                self.mode = "EXPLORE"
                self.stuck_count = max(0, self.stuck_count - 1)
        
        elif self.mode == "RECOVER":
            # Recovery mode - back up and turn
            if (current_time - self.last_turn_time) > 2.0:
                self.mode = "EXPLORE"
    
    def _update_position(self, dt: float):
        """Update truck position and heading based on behavior."""
        # Determine movement parameters
        if self.mode == "EXPLORE":
            target_speed = 0.3  # m/s
            angular_velocity = 0.0
            
        elif self.mode == "AVOID" or self.mode == "TURN":
            target_speed = 0.1  # Slower while turning
            # Turn toward target heading
            heading_error = self._angle_difference(self.target_heading, self.heading)
            angular_velocity = math.copysign(min(abs(heading_error) * 2.0, 90.0), heading_error)
            
        elif self.mode == "RECOVER":
            target_speed = -0.1  # Back up
            angular_velocity = 45.0  # Turn while backing up
            
        else:
            target_speed = 0.0
            angular_velocity = 0.0
        
        # Apply some randomness
        target_speed += random.uniform(-0.05, 0.05)
        angular_velocity += random.uniform(-10, 10)
        
        # Update speed with simple acceleration model
        speed_diff = target_speed - self.speed
        acceleration = 2.0  # m/s^2
        self.speed += math.copysign(min(abs(speed_diff), acceleration * dt), speed_diff)
        
        # Update heading
        self.heading = (self.heading + angular_velocity * dt) % 360
        
        # Update position
        heading_rad = math.radians(self.heading)
        dx = self.speed * math.cos(heading_rad) * dt
        dy = self.speed * math.sin(heading_rad) * dt
        
        self.x += dx
        self.y += dy
        
        # Keep within arena bounds (with some buffer)
        self.x = max(0.5, min(self.arena_width - 0.5, self.x))
        self.y = max(0.5, min(self.arena_height - 0.5, self.y))
    
    def _is_near_boundary(self) -> bool:
        """Check if truck is near arena boundaries."""
        margin = 1.0  # meters
        return (self.x < margin or self.x > self.arena_width - margin or 
                self.y < margin or self.y > self.arena_height - margin)
    
    def _angle_difference(self, target: float, current: float) -> float:
        """Calculate the shortest angular difference between two angles."""
        diff = target - current
        while diff > 180:
            diff -= 360
        while diff < -180:
            diff += 360
        return diff
    
    def _get_sensor_readings(self) -> dict:
        """Simulate ultrasonic sensor readings based on current position."""
        sensors = {}
        
        # Sensor positions relative to truck center (in meters)
        sensor_offsets = {
            'uf': (0.15, 0.0),   # Front
            'ub': (-0.15, 0.0),  # Back  
            'ul': (0.0, 0.10),   # Left
            'ur': (0.0, -0.10)   # Right
        }
        
        # Sensor directions relative to truck heading
        sensor_angles = {
            'uf': 0.0,      # Forward
            'ub': 180.0,    # Backward
            'ul': 90.0,     # Left
            'ur': -90.0     # Right
        }
        
        for sensor_name, (offset_x, offset_y) in sensor_offsets.items():
            # Calculate sensor world position
            heading_rad = math.radians(self.heading)
            cos_h, sin_h = math.cos(heading_rad), math.sin(heading_rad)
            
            sensor_x = self.x + offset_x * cos_h - offset_y * sin_h
            sensor_y = self.y + offset_x * sin_h + offset_y * cos_h
            
            # Calculate sensor beam direction
            sensor_heading = (self.heading + sensor_angles[sensor_name]) % 360
            beam_rad = math.radians(sensor_heading)
            
            # Simulate range finding
            distance_cm = self._simulate_range_measurement(
                sensor_x, sensor_y, beam_rad
            )
            
            sensors[sensor_name] = distance_cm
        
        return sensors
    
    def _simulate_range_measurement(self, x: float, y: float, beam_angle: float) -> float:
        """Simulate a single ultrasonic range measurement."""
        max_range = 2.0  # meters
        beam_dx = math.cos(beam_angle)
        beam_dy = math.sin(beam_angle)
        
        # Check for boundary collision
        min_distance = max_range
        
        # Check arena boundaries
        if beam_dx > 0:  # Moving right
            dist_to_right = (self.arena_width - x) / beam_dx
            if dist_to_right > 0:
                min_distance = min(min_distance, dist_to_right)
                
        elif beam_dx < 0:  # Moving left
            dist_to_left = -x / beam_dx  
            if dist_to_left > 0:
                min_distance = min(min_distance, dist_to_left)
        
        if beam_dy > 0:  # Moving up
            dist_to_top = (self.arena_height - y) / beam_dy
            if dist_to_top > 0:
                min_distance = min(min_distance, dist_to_top)
                
        elif beam_dy < 0:  # Moving down
            dist_to_bottom = -y / beam_dy
            if dist_to_bottom > 0:
                min_distance = min(min_distance, dist_to_bottom)
        
        # Check obstacles
        for obs_x, obs_y, obs_w, obs_h in self.obstacles:
            obs_dist = self._ray_box_intersection(
                x, y, beam_dx, beam_dy,
                obs_x, obs_y, obs_w, obs_h
            )
            if obs_dist is not None and obs_dist < min_distance:
                min_distance = obs_dist
        
        # Add some noise
        min_distance += random.uniform(-0.02, 0.02)
        
        # Convert to centimeters and clamp
        distance_cm = min_distance * 100.0
        
        # Simulate sensor limitations
        if distance_cm > 180.0:
            return 199.0  # Max range reading
        elif distance_cm < 2.0:
            return random.uniform(2.0, 5.0)  # Minimum range noise
        else:
            return max(2.0, distance_cm)
    
    def _ray_box_intersection(self, ray_x: float, ray_y: float, 
                             ray_dx: float, ray_dy: float,
                             box_x: float, box_y: float, 
                             box_w: float, box_h: float) -> float:
        """Calculate ray-box intersection distance."""
        # Simple AABB ray intersection
        if ray_dx == 0 and ray_dy == 0:
            return None
            
        t_min = 0
        t_max = float('inf')
        
        # X direction
        if ray_dx != 0:
            t1 = (box_x - ray_x) / ray_dx
            t2 = (box_x + box_w - ray_x) / ray_dx
            
            t_min = max(t_min, min(t1, t2))
            t_max = min(t_max, max(t1, t2))
        else:
            if ray_x < box_x or ray_x > box_x + box_w:
                return None
        
        # Y direction
        if ray_dy != 0:
            t1 = (box_y - ray_y) / ray_dy
            t2 = (box_y + box_h - ray_y) / ray_dy
            
            t_min = max(t_min, min(t1, t2))
            t_max = min(t_max, max(t1, t2))
        else:
            if ray_y < box_y or ray_y > box_y + box_h:
                return None
        
        if t_min <= t_max and t_min > 0:
            return t_min
        
        return None
    
    def _generate_telemetry_packet(self) -> dict:
        """Generate a telemetry packet with current simulation state."""
        sensors = self._get_sensor_readings()
        
        # Convert speed to PWM (rough approximation)
        pwm = int(abs(self.speed) * 100 / 0.5)  # Scale to 0-100
        pwm = max(0, min(100, pwm))
        
        # Determine steering command
        if self.mode == "TURN" or self.mode == "AVOID":
            heading_error = self._angle_difference(self.target_heading, self.heading)
            if abs(heading_error) > 5:
                steer = "LEFT" if heading_error > 0 else "RIGHT"
            else:
                steer = "STRAIGHT"
        else:
            steer = "STRAIGHT"
        
        # Determine mode
        if self.stuck_count > 3:
            mode = "RECOVER"
        elif self.mode == "AVOID":
            mode = "AVOID"
        else:
            mode = "EXPLORE"
        
        packet = {
            "truck_id": self.truck_id,
            "seq": self.seq,
            "t_ms": self.t_ms,
            "free_heap": random.randint(200000, 250000),
            "mode": mode,
            
            # Sensor readings
            "ul": sensors['ul'],
            "ur": sensors['ur'],
            "uf": sensors['uf'], 
            "ub": sensors['ub'],
            
            # Motion data with some noise
            "yaw_rate": random.uniform(-2, 2),
            "gy_x": random.uniform(-30, 30),
            "gy_y": random.uniform(-30, 30),
            "gy_z": random.uniform(200, 300),
            "heading": self.heading % 360,
            "compass": (self.heading + random.uniform(-2, 2)) % 360,
            
            # Magnetometer
            "mag_x": random.uniform(8, 12),
            "mag_y": random.uniform(9, 13),
            "mag_z": random.uniform(-40, -30),
            
            # Accelerometer  
            "acc_x": random.uniform(-200, 200),
            "acc_y": random.uniform(4000, 4800),
            "acc_z": random.uniform(15500, 16500),
            
            # Line following (not used in this sim)
            "width": random.uniform(350, 450),
            "center_error": random.uniform(-10, 10),
            "front_blocked": sensors['uf'] < 25.0,
            
            # Commands
            "cmd_pwm": pwm,
            "cmd_steer": steer
        }
        
        return packet
    
    def _send_telemetry(self, packet: dict):
        """Send telemetry packet to the server."""
        try:
            response = requests.post(
                self.endpoint,
                json=packet,
                timeout=1.0
            )
            
            if response.status_code == 200:
                if self.seq % 50 == 0:  # Print every 50th packet
                    print(f"Sent packet {self.seq} - Mode: {packet['mode']} - "
                          f"Pos: ({self.x:.2f}, {self.y:.2f}) - "
                          f"Heading: {self.heading:.1f}°")
            else:
                print(f"Server error: {response.status_code} - {response.text}")
                
        except requests.exceptions.RequestException as e:
            if self.seq % 100 == 0:  # Don't spam connection errors
                print(f"Connection error: {e}")


def main():
    """Main function to run the telemetry simulator."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Truck Telemetry Simulator")
    parser.add_argument('--server', default='http://localhost:5000',
                       help='Server URL (default: http://localhost:5000)')
    parser.add_argument('--rate', type=float, default=10.0,
                       help='Update rate in Hz (default: 10.0)')
    
    args = parser.parse_args()
    
    # Create and start simulator
    simulator = TelemetrySimulator(server_url=args.server)
    simulator.update_rate = args.rate
    
    # Start simulation
    simulator.start()


if __name__ == '__main__':
    main()