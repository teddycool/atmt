"""
Occupancy grid mapping for the truck mapping system.
"""

import math
import numpy as np
from typing import Tuple, List, Optional
from core.telemetry import TelemetryPacket
from core.pose import Pose, PoseEstimator


class OccupancyGrid:
    """2D occupancy grid using log-odds representation."""
    
    def __init__(self, config):
        self.config = config
        
        # Calculate grid dimensions
        self.width_cells = int(config.MAP_WIDTH_M / config.CELL_SIZE_M)
        self.height_cells = int(config.MAP_HEIGHT_M / config.CELL_SIZE_M)
        
        # Initialize grid with zeros (unknown)
        self.log_odds = np.zeros((self.height_cells, self.width_cells), dtype=np.float32)
        
        # Map origin (where world coordinates (0,0) maps to in the grid)
        self.origin_x = config.MAP_CENTER_X - config.MAP_WIDTH_M / 2
        self.origin_y = config.MAP_CENTER_Y - config.MAP_HEIGHT_M / 2
        
        print(f"Created occupancy grid: {self.width_cells}x{self.height_cells} cells")
        print(f"Resolution: {config.CELL_SIZE_M}m per cell")
        print(f"Map bounds: ({self.origin_x:.2f}, {self.origin_y:.2f}) to "
              f"({self.origin_x + config.MAP_WIDTH_M:.2f}, {self.origin_y + config.MAP_HEIGHT_M:.2f})")
    
    def world_to_grid(self, world_x: float, world_y: float) -> Tuple[int, int]:
        """Convert world coordinates to grid indices."""
        grid_x = int((world_x - self.origin_x) / self.config.CELL_SIZE_M)
        grid_y = int((world_y - self.origin_y) / self.config.CELL_SIZE_M)
        return grid_x, grid_y
    
    def grid_to_world(self, grid_x: int, grid_y: int) -> Tuple[float, float]:
        """Convert grid indices to world coordinates (cell center)."""
        world_x = self.origin_x + (grid_x + 0.5) * self.config.CELL_SIZE_M
        world_y = self.origin_y + (grid_y + 0.5) * self.config.CELL_SIZE_M
        return world_x, world_y
    
    def is_valid_cell(self, grid_x: int, grid_y: int) -> bool:
        """Check if grid coordinates are within bounds."""
        return 0 <= grid_x < self.width_cells and 0 <= grid_y < self.height_cells
    
    def update_sensor_ray(self, sensor_x: float, sensor_y: float, 
                         sensor_angle: float, range_reading: float):
        """
        Update occupancy grid based on a sensor reading.
        
        Args:
            sensor_x: Sensor world X position
            sensor_y: Sensor world Y position  
            sensor_angle: Sensor beam angle in radians
            range_reading: Range reading in cm
        """
        # Convert range from cm to meters
        range_m = range_reading / 100.0
        
        # Check if reading is valid
        max_range_m = self.config.SENSOR_MAX_RANGE_CM / 100.0
        min_range_m = self.config.SENSOR_MIN_RANGE_CM / 100.0
        
        # If reading indicates no valid return (like 199.0), 
        # mark free space up to max range
        if range_reading >= self.config.SENSOR_INVALID_VALUE:
            range_m = max_range_m
            mark_endpoint_occupied = False
        elif range_m < min_range_m:
            # Too close reading, ignore
            return
        elif range_m > max_range_m:
            # Clamp to max range
            range_m = max_range_m
            mark_endpoint_occupied = False
        else:
            # Valid reading, mark endpoint as occupied
            mark_endpoint_occupied = True
        
        # Calculate endpoint
        end_x = sensor_x + range_m * math.cos(sensor_angle)
        end_y = sensor_y + range_m * math.sin(sensor_angle)
        
        # Get grid coordinates
        start_gx, start_gy = self.world_to_grid(sensor_x, sensor_y)
        end_gx, end_gy = self.world_to_grid(end_x, end_y)
        
        # Trace ray from sensor to endpoint
        ray_cells = self._bresenham_line(start_gx, start_gy, end_gx, end_gy)
        
        # Mark free cells along the ray (except possibly the endpoint)
        for i, (gx, gy) in enumerate(ray_cells):
            if self.is_valid_cell(gx, gy):
                is_endpoint = (i == len(ray_cells) - 1)
                
                if is_endpoint and mark_endpoint_occupied:
                    # Mark endpoint as occupied
                    self._update_cell(gx, gy, self.config.LOG_ODDS_OCCUPIED)
                else:
                    # Mark as free space
                    self._update_cell(gx, gy, self.config.LOG_ODDS_FREE)
    
    def _update_cell(self, grid_x: int, grid_y: int, log_odds_update: float):
        """Update a single cell's log-odds value."""
        self.log_odds[grid_y, grid_x] += log_odds_update
        
        # Clamp to avoid overflow
        self.log_odds[grid_y, grid_x] = np.clip(
            self.log_odds[grid_y, grid_x],
            self.config.LOG_ODDS_CLAMP_MIN,
            self.config.LOG_ODDS_CLAMP_MAX
        )
    
    def _bresenham_line(self, x0: int, y0: int, x1: int, y1: int) -> List[Tuple[int, int]]:
        """Generate line points using Bresenham's algorithm."""
        points = []
        
        dx = abs(x1 - x0)
        dy = abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        
        x, y = x0, y0
        
        while True:
            points.append((x, y))
            
            if x == x1 and y == y1:
                break
                
            e2 = 2 * err
            
            if e2 > -dy:
                err -= dy
                x += sx
                
            if e2 < dx:
                err += dx
                y += sy
        
        return points
    
    def get_probability_grid(self) -> np.ndarray:
        """Convert log-odds to probability values (0-1)."""
        # Convert log-odds to probability: p = 1 / (1 + exp(-log_odds))
        # Use sigmoid function with numerical stability
        probabilities = np.zeros_like(self.log_odds)
        
        # For positive log-odds
        positive_mask = self.log_odds >= 0
        probabilities[positive_mask] = 1.0 / (1.0 + np.exp(-self.log_odds[positive_mask]))
        
        # For negative log-odds (more numerically stable)
        negative_mask = self.log_odds < 0
        exp_logodds = np.exp(self.log_odds[negative_mask])
        probabilities[negative_mask] = exp_logodds / (1.0 + exp_logodds)
        
        return probabilities
    
    def reset(self):
        """Reset the occupancy grid to unknown state."""
        self.log_odds.fill(0.0)


class OccupancyMapper:
    """Manages occupancy grid mapping using telemetry data."""
    
    def __init__(self, config, pose_estimator: PoseEstimator):
        self.config = config
        self.pose_estimator = pose_estimator
        self.grid = OccupancyGrid(config)
        self.update_count = 0
    
    def update(self, packet: TelemetryPacket, pose: Pose):
        """Update the map with new telemetry data."""
        # Get sensor positions in world coordinates
        sensor_positions = self.pose_estimator.get_sensor_positions()
        
        # Update map for each sensor
        sensors_data = {
            'uf': packet.uf,
            'ub': packet.ub, 
            'ul': packet.ul,
            'ur': packet.ur
        }
        
        for sensor_name, range_reading in sensors_data.items():
            if sensor_name in sensor_positions:
                sensor_x, sensor_y, sensor_angle = sensor_positions[sensor_name]
                self.grid.update_sensor_ray(sensor_x, sensor_y, sensor_angle, range_reading)
        
        self.update_count += 1
    
    def get_grid(self) -> OccupancyGrid:
        """Get the current occupancy grid."""
        return self.grid
    
    def reset(self):
        """Reset the map to empty state."""
        self.grid.reset()
        self.update_count = 0
    
    def get_stats(self) -> dict:
        """Get mapping statistics."""
        prob_grid = self.grid.get_probability_grid()
        
        # Count cells in different categories
        unknown_cells = np.sum(np.abs(self.grid.log_odds) < 0.1)
        free_cells = np.sum(prob_grid < 0.4)
        occupied_cells = np.sum(prob_grid > 0.6)
        total_cells = self.grid.width_cells * self.grid.height_cells
        
        return {
            'update_count': self.update_count,
            'total_cells': total_cells,
            'unknown_cells': int(unknown_cells),
            'free_cells': int(free_cells),
            'occupied_cells': int(occupied_cells),
            'grid_size': (self.grid.width_cells, self.grid.height_cells),
            'map_bounds': (self.grid.origin_x, self.grid.origin_y, 
                          self.grid.origin_x + self.config.MAP_WIDTH_M,
                          self.grid.origin_y + self.config.MAP_HEIGHT_M)
        }