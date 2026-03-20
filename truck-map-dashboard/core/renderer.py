"""
Map rendering for the truck mapping dashboard.
"""

import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from typing import Tuple, Optional
from core.occupancy_grid import OccupancyGrid
from core.pose import Pose


class MapRenderer:
    """Renders occupancy grid maps as images for display."""
    
    def __init__(self, config):
        self.config = config
        
        # Color scheme
        self.colors = {
            'unknown': (128, 128, 128),     # Gray for unknown space
            'free': (255, 255, 255),        # White for free space
            'occupied': (0, 0, 0),          # Black for obstacles
            'truck': (255, 0, 0),           # Red for truck position
            'truck_heading': (255, 100, 100), # Light red for heading indicator
            'trajectory': (0, 255, 0),      # Green for trajectory
            'sensor_rays': (255, 255, 0)   # Yellow for sensor rays
        }
        
        # Try to load a font (fallback to default if not available)
        try:
            self.font = ImageFont.truetype("DejaVuSans.ttf", 12)
        except:
            try:
                self.font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 12)
            except:
                self.font = ImageFont.load_default()
    
    def render_map(self, grid: OccupancyGrid, pose: Optional[Pose] = None, 
                   show_truck: bool = True, show_grid: bool = False) -> Image.Image:
        """
        Render the occupancy grid as an image.
        
        Args:
            grid: OccupancyGrid to render
            pose: Current truck pose (optional)
            show_truck: Whether to show truck position and heading
            show_grid: Whether to show grid lines
            
        Returns:
            PIL Image of the rendered map
        """
        # Create image
        img_width = self.config.MAP_IMAGE_WIDTH
        img_height = self.config.MAP_IMAGE_HEIGHT
        
        # Get probability grid
        prob_grid = grid.get_probability_grid()
        
        # Convert probabilities to colors
        color_image = self._probabilities_to_colors(prob_grid)
        
        # Resize to target dimensions
        img = Image.fromarray(color_image, 'RGB')
        img = img.resize((img_width, img_height), Image.NEAREST)
        
        # Convert to RGB for drawing
        draw = ImageDraw.Draw(img)
        
        # Add grid lines if requested
        if show_grid:
            self._draw_grid_lines(draw, img_width, img_height, grid)
        
        # Add truck position and heading if provided
        if show_truck and pose is not None:
            self._draw_truck(draw, pose, grid, img_width, img_height)
        
        # Add coordinate labels
        self._draw_coordinate_labels(draw, grid, img_width, img_height)
        
        return img
    
    def _probabilities_to_colors(self, prob_grid: np.ndarray) -> np.ndarray:
        """Convert probability grid to RGB color array."""
        height, width = prob_grid.shape
        color_image = np.zeros((height, width, 3), dtype=np.uint8)
        
        # Unknown space (probability ~0.5)
        unknown_mask = (prob_grid >= 0.4) & (prob_grid <= 0.6)
        color_image[unknown_mask] = self.colors['unknown']
        
        # Free space (low probability)
        free_mask = prob_grid < 0.4
        color_image[free_mask] = self.colors['free']
        
        # Occupied space (high probability)
        occupied_mask = prob_grid > 0.6
        color_image[occupied_mask] = self.colors['occupied']
        
        # Flip vertically to match image coordinate system
        color_image = np.flipud(color_image)
        
        return color_image
    
    def _draw_grid_lines(self, draw: ImageDraw.Draw, img_width: int, img_height: int, grid: OccupancyGrid):
        """Draw grid lines on the image."""
        # Calculate cell size in pixels
        cell_size_x = img_width / grid.width_cells
        cell_size_y = img_height / grid.height_cells
        
        # Draw vertical lines
        for i in range(0, grid.width_cells + 1, 10):  # Every 10 cells
            x = i * cell_size_x
            draw.line([(x, 0), (x, img_height)], fill=(200, 200, 200), width=1)
        
        # Draw horizontal lines
        for j in range(0, grid.height_cells + 1, 10):  # Every 10 cells
            y = j * cell_size_y
            draw.line([(0, y), (img_width, y)], fill=(200, 200, 200), width=1)
    
    def _draw_truck(self, draw: ImageDraw.Draw, pose: Pose, grid: OccupancyGrid, 
                    img_width: int, img_height: int):
        """Draw truck position and heading on the map."""
        # Convert world coordinates to image coordinates
        img_x, img_y = self._world_to_image(pose.x, pose.y, grid, img_width, img_height)
        
        # Draw truck position as a circle
        marker_size = self.config.TRUCK_MARKER_SIZE
        draw.ellipse([
            img_x - marker_size // 2, 
            img_y - marker_size // 2,
            img_x + marker_size // 2, 
            img_y + marker_size // 2
        ], fill=self.colors['truck'], outline='black', width=2)
        
        # Draw heading indicator
        heading_length = marker_size * 2
        end_x = img_x + heading_length * math.cos(pose.theta)
        end_y = img_y - heading_length * math.sin(pose.theta)  # Minus because image Y is flipped
        
        draw.line([img_x, img_y, end_x, end_y], 
                 fill=self.colors['truck_heading'], width=3)
        
        # Draw heading angle text
        heading_deg = pose.get_heading_degrees()
        text = f"{heading_deg:.1f}°"
        draw.text((img_x + marker_size, img_y - marker_size), text, 
                 fill=self.colors['truck'], font=self.font)
    
    def _draw_coordinate_labels(self, draw: ImageDraw.Draw, grid: OccupancyGrid, 
                               img_width: int, img_height: int):
        """Draw coordinate system labels on the map."""
        # Draw origin marker if visible
        origin_img_x, origin_img_y = self._world_to_image(0, 0, grid, img_width, img_height)
        
        if 0 <= origin_img_x <= img_width and 0 <= origin_img_y <= img_height:
            # Draw small cross at origin
            cross_size = 5
            draw.line([
                origin_img_x - cross_size, origin_img_y,
                origin_img_x + cross_size, origin_img_y
            ], fill=(255, 0, 255), width=2)
            draw.line([
                origin_img_x, origin_img_y - cross_size,
                origin_img_x, origin_img_y + cross_size
            ], fill=(255, 0, 255), width=2)
            draw.text((origin_img_x + 8, origin_img_y - 15), "(0,0)", 
                     fill=(255, 0, 255), font=self.font)
        
        # Draw scale information
        scale_text = f"Scale: {self.config.CELL_SIZE_M*100:.0f} cm/cell"
        draw.text((10, img_height - 25), scale_text, fill='black', font=self.font)
        
        # Draw map bounds
        bounds_text = f"Map: {grid.origin_x:.1f}m to {grid.origin_x + self.config.MAP_WIDTH_M:.1f}m (X)"
        draw.text((10, 10), bounds_text, fill='black', font=self.font)
    
    def _world_to_image(self, world_x: float, world_y: float, grid: OccupancyGrid, 
                        img_width: int, img_height: int) -> Tuple[int, int]:
        """Convert world coordinates to image pixel coordinates."""
        # Convert to grid coordinates
        grid_x, grid_y = grid.world_to_grid(world_x, world_y)
        
        # Convert grid coordinates to image coordinates
        img_x = int((grid_x / grid.width_cells) * img_width)
        img_y = int(img_height - (grid_y / grid.height_cells) * img_height)  # Flip Y
        
        return img_x, img_y
    
    def render_debug_info(self, grid: OccupancyGrid, pose: Pose, stats: dict) -> Image.Image:
        """Render a debug information overlay."""
        img_width = 300
        img_height = 200
        
        # Create white background
        img = Image.new('RGB', (img_width, img_height), 'white')
        draw = ImageDraw.Draw(img)
        
        # Debug information
        y_pos = 10
        line_height = 15
        
        debug_lines = [
            f"Pose: ({pose.x:.2f}, {pose.y:.2f})",
            f"Heading: {pose.get_heading_degrees():.1f}°",
            f"Grid: {grid.width_cells}x{grid.height_cells}",
            f"Updates: {stats.get('update_count', 0)}",
            f"Unknown: {stats.get('unknown_cells', 0)}",
            f"Free: {stats.get('free_cells', 0)}",
            f"Occupied: {stats.get('occupied_cells', 0)}",
            f"Total packets: {stats.get('packet_count', 0)}",
            f"Last seq: {stats.get('last_seq', 'N/A')}",
            f"Mode: {stats.get('last_mode', 'N/A')}"
        ]
        
        for line in debug_lines:
            draw.text((10, y_pos), line, fill='black', font=self.font)
            y_pos += line_height
        
        return img
    
    def save_map_png(self, grid: OccupancyGrid, pose: Pose, filename: str):
        """Save current map as PNG file."""
        img = self.render_map(grid, pose, show_truck=True, show_grid=True)
        img.save(filename, 'PNG')
        print(f"Map saved to {filename}")