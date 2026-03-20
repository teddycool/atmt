"""
Database operations for telemetry logging.
"""

import sqlite3
import json
import time
from typing import List, Dict, Any, Optional
from contextlib import contextmanager
from core.telemetry import TelemetryPacket


class DatabaseManager:
    """Manages SQLite database operations for telemetry logging."""
    
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._create_tables()
    
    def _create_tables(self):
        """Create database tables if they don't exist."""
        with self.get_connection() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    truck_id TEXT NOT NULL,
                    seq INTEGER NOT NULL,
                    t_ms INTEGER NOT NULL,
                    received_at REAL NOT NULL,
                    mode TEXT,
                    
                    -- Sensor readings
                    ul REAL,
                    ur REAL, 
                    uf REAL,
                    ub REAL,
                    
                    -- Motion data
                    heading REAL,
                    yaw_rate REAL,
                    compass REAL,
                    
                    -- Control commands
                    cmd_pwm INTEGER,
                    cmd_steer TEXT,
                    
                    -- Raw JSON for full data
                    raw_json TEXT,
                    
                    -- Indexes for common queries
                    UNIQUE(truck_id, seq)
                )
            """)
            
            # Create indexes for performance
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_truck_seq 
                ON telemetry(truck_id, seq)
            """)
            
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_received_at 
                ON telemetry(received_at)
            """)
            
            # Create pose estimates table
            conn.execute("""
                CREATE TABLE IF NOT EXISTS pose_estimates (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    truck_id TEXT NOT NULL,
                    seq INTEGER NOT NULL,
                    timestamp REAL NOT NULL,
                    x REAL NOT NULL,
                    y REAL NOT NULL,
                    theta REAL NOT NULL,
                    
                    FOREIGN KEY(truck_id, seq) REFERENCES telemetry(truck_id, seq)
                )
            """)
            
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_pose_timestamp 
                ON pose_estimates(timestamp)
            """)
    
    @contextmanager
    def get_connection(self):
        """Get a database connection with automatic cleanup."""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row  # Enable dict-like access
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            conn.close()
    
    def insert_telemetry(self, packet: TelemetryPacket) -> bool:
        """
        Insert a telemetry packet into the database.
        
        Args:
            packet: TelemetryPacket to store
            
        Returns:
            bool: True if inserted successfully, False if duplicate
        """
        raw_json = self._packet_to_json(packet)
        
        try:
            with self.get_connection() as conn:
                conn.execute("""
                    INSERT INTO telemetry (
                        truck_id, seq, t_ms, received_at, mode,
                        ul, ur, uf, ub, heading, yaw_rate, compass,
                        cmd_pwm, cmd_steer, raw_json
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    packet.truck_id,
                    packet.seq,
                    packet.t_ms,
                    packet.received_at,
                    packet.mode,
                    packet.ul,
                    packet.ur,
                    packet.uf,
                    packet.ub,
                    packet.heading,
                    packet.yaw_rate,
                    packet.compass,
                    packet.cmd_pwm,
                    packet.cmd_steer,
                    raw_json
                ))
                return True
                
        except sqlite3.IntegrityError:
            # Duplicate sequence number for this truck
            return False
    
    def insert_pose(self, truck_id: str, seq: int, pose, timestamp: float) -> bool:
        """Insert a pose estimate into the database."""
        try:
            with self.get_connection() as conn:
                conn.execute("""
                    INSERT INTO pose_estimates (
                        truck_id, seq, timestamp, x, y, theta
                    ) VALUES (?, ?, ?, ?, ?, ?)
                """, (
                    truck_id, seq, timestamp, pose.x, pose.y, pose.theta
                ))
                return True
        except sqlite3.Error:
            return False
    
    def get_recent_telemetry(self, truck_id: str, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent telemetry packets for a truck."""
        with self.get_connection() as conn:
            cursor = conn.execute("""
                SELECT * FROM telemetry 
                WHERE truck_id = ?
                ORDER BY seq DESC
                LIMIT ?
            """, (truck_id, limit))
            
            return [dict(row) for row in cursor.fetchall()]
    
    def get_latest_telemetry(self, truck_id: str) -> Optional[Dict[str, Any]]:
        """Get the most recent telemetry packet for a truck."""
        recent = self.get_recent_telemetry(truck_id, limit=1)
        return recent[0] if recent else None
    
    def get_telemetry_stats(self) -> Dict[str, Any]:
        """Get telemetry database statistics."""
        with self.get_connection() as conn:
            # Total packet count
            cursor = conn.execute("SELECT COUNT(*) as total FROM telemetry")
            total = cursor.fetchone()['total']
            
            # Unique trucks
            cursor = conn.execute("SELECT COUNT(DISTINCT truck_id) as unique_trucks FROM telemetry")
            unique_trucks = cursor.fetchone()['unique_trucks']
            
            # Latest packet time
            cursor = conn.execute("SELECT MAX(received_at) as latest FROM telemetry")
            latest = cursor.fetchone()['latest']
            
            # Oldest packet time
            cursor = conn.execute("SELECT MIN(received_at) as oldest FROM telemetry")
            oldest = cursor.fetchone()['oldest']
            
            return {
                'total_packets': total,
                'unique_trucks': unique_trucks,
                'latest_packet_time': latest,
                'oldest_packet_time': oldest,
                'duration_hours': (latest - oldest) / 3600.0 if latest and oldest else 0.0
            }
    
    def clear_telemetry(self, truck_id: str = None):
        """Clear telemetry data, optionally for a specific truck."""
        with self.get_connection() as conn:
            if truck_id:
                conn.execute("DELETE FROM telemetry WHERE truck_id = ?", (truck_id,))
                conn.execute("DELETE FROM pose_estimates WHERE truck_id = ?", (truck_id,))
            else:
                conn.execute("DELETE FROM telemetry")
                conn.execute("DELETE FROM pose_estimates")
    
    def _packet_to_json(self, packet: TelemetryPacket) -> str:
        """Convert packet to JSON string for storage."""
        # Convert packet to dict, excluding received_at since we store it separately
        packet_dict = {}
        for field in packet.__dataclass_fields__:
            if field != 'received_at':
                value = getattr(packet, field)
                packet_dict[field] = value
                
        return json.dumps(packet_dict, default=str)
    
    def export_telemetry_csv(self, truck_id: str, filename: str):
        """Export telemetry data to CSV file."""
        with self.get_connection() as conn:
            cursor = conn.execute("""
                SELECT truck_id, seq, t_ms, received_at, mode,
                       ul, ur, uf, ub, heading, yaw_rate, compass,
                       cmd_pwm, cmd_steer
                FROM telemetry 
                WHERE truck_id = ?
                ORDER BY seq
            """, (truck_id,))
            
            import csv
            with open(filename, 'w', newline='') as csvfile:
                writer = csv.writer(csvfile)
                
                # Write header
                writer.writerow([desc[0] for desc in cursor.description])
                
                # Write data
                writer.writerows(cursor.fetchall())