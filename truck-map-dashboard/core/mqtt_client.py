"""
MQTT client for receiving telemetry data from the truck.
"""

import json
import time
import threading
from typing import Callable, Optional
import paho.mqtt.client as mqtt


class MQTTTelemetryClient:
    """MQTT client for receiving truck telemetry data."""
    
    def __init__(self, config, telemetry_callback: Callable[[str], None]):
        self.config = config
        self.telemetry_callback = telemetry_callback
        
        # MQTT client setup
        self.client = mqtt.Client()
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        self.client.on_subscribe = self._on_subscribe
        
        # Connection state
        self.is_connected = False
        self.connection_attempts = 0
        self.last_message_time = None
        self.message_count = 0
        
        # Stats
        self.stats = {
            'connected': False,
            'connection_attempts': 0,
            'messages_received': 0,
            'last_message_time': None,
            'errors': 0
        }
        
        print(f"MQTT Client initialized")
        print(f"Broker: {config.MQTT_BROKER_HOST}:{config.MQTT_BROKER_PORT}")
        print(f"Topic: {config.MQTT_TOPIC}")
    
    def start(self):
        """Start the MQTT client and connect to broker."""
        try:
            print("Connecting to MQTT broker...")
            self.connection_attempts += 1
            self.stats['connection_attempts'] = self.connection_attempts
            
            # Configure authentication if provided
            if self.config.MQTT_USERNAME and self.config.MQTT_PASSWORD:
                self.client.username_pw_set(
                    self.config.MQTT_USERNAME, 
                    self.config.MQTT_PASSWORD
                )
            
            # Connect to broker
            self.client.connect(
                self.config.MQTT_BROKER_HOST,
                self.config.MQTT_BROKER_PORT,
                self.config.MQTT_KEEPALIVE
            )
            
            # Start the network loop in a separate thread
            self.client.loop_start()
            
            return True
            
        except Exception as e:
            print(f"Failed to connect to MQTT broker: {e}")
            self.stats['errors'] += 1
            return False
    
    def stop(self):
        """Stop the MQTT client."""
        print("Stopping MQTT client...")
        try:
            self.client.loop_stop()
            self.client.disconnect()
            self.is_connected = False
            self.stats['connected'] = False
            print("MQTT client stopped")
        except Exception as e:
            print(f"Error stopping MQTT client: {e}")
    
    def _on_connect(self, client, userdata, flags, rc):
        """Callback for when the client receives a CONNACK response from the server."""
        if rc == 0:
            self.is_connected = True
            self.stats['connected'] = True
            print(f"Connected to MQTT broker with result code: {rc}")
            
            # Subscribe to telemetry topic
            topic = self.config.MQTT_TOPIC
            print(f"Subscribing to topic: {topic}")
            result = client.subscribe(topic)
            print(f"Subscription result: {result}")
            
        else:
            self.is_connected = False
            self.stats['connected'] = False
            print(f"Failed to connect to MQTT broker with result code: {rc}")
            self.stats['errors'] += 1
    
    def _on_disconnect(self, client, userdata, rc):
        """Callback for when the client disconnects from the server."""
        self.is_connected = False
        self.stats['connected'] = False
        if rc != 0:
            print(f"Unexpected MQTT disconnection. Code: {rc}")
        else:
            print("MQTT client disconnected")
    
    def _on_subscribe(self, client, userdata, mid, granted_qos):
        """Callback for when the client receives a SUBACK response from the server."""
        print(f"Subscribed to topic with QoS: {granted_qos}")
    
    def _on_message(self, client, userdata, msg):
        """Callback for when a PUBLISH message is received from the server."""
        try:
            # Decode message
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            
            print(f"Received MQTT message on topic: {topic}")
            print(f"Payload length: {len(payload)} characters")
            
            # Update stats
            self.message_count += 1
            self.last_message_time = time.time()
            self.stats['messages_received'] = self.message_count
            self.stats['last_message_time'] = self.last_message_time
            
            # Call the telemetry processing callback
            if self.telemetry_callback:
                self.telemetry_callback(payload)
            
        except Exception as e:
            print(f"Error processing MQTT message: {e}")
            self.stats['errors'] += 1
    
    def get_status(self) -> dict:
        """Get current MQTT client status."""
        return {
            'connected': self.is_connected,
            'broker': f"{self.config.MQTT_BROKER_HOST}:{self.config.MQTT_BROKER_PORT}",
            'topic': self.config.MQTT_TOPIC,
            'stats': self.stats.copy()
        }
    
    def publish_test_message(self, test_data: dict):
        """Publish a test message (for debugging)."""
        if not self.is_connected:
            print("Cannot publish: MQTT client not connected")
            return False
        
        try:
            topic = self.config.MQTT_TOPIC
            payload = json.dumps(test_data)
            result = self.client.publish(topic, payload)
            print(f"Published test message to {topic}")
            return result.rc == 0
        except Exception as e:
            print(f"Error publishing test message: {e}")
            return False