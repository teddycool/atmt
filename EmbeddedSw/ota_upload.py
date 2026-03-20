#!/usr/bin/env python3
import socket
import struct
import hashlib
import os
import sys

def esp32_ota_upload(host, port, firmware_path):
    """Upload firmware to ESP32 via Arduino OTA protocol"""
    
    if not os.path.exists(firmware_path):
        print(f"Firmware file not found: {firmware_path}")
        return False
    
    # Read firmware file
    with open(firmware_path, 'rb') as f:
        firmware = f.read()
    
    print(f"Connecting to {host}:{port}")
    print(f"Firmware size: {len(firmware)} bytes")
    
    try:
        # Create socket connection
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30)  # 30 second timeout
        sock.connect((host, port))
        
        # Send OTA command
        sock.send(b'\x01')  # OTA begin command
        
        # Send firmware size (4 bytes, little endian)
        sock.send(struct.pack('<I', len(firmware)))
        
        # Send firmware data in chunks
        chunk_size = 1024
        bytes_sent = 0
        
        for i in range(0, len(firmware), chunk_size):
            chunk = firmware[i:i+chunk_size]
            sock.send(chunk)
            bytes_sent += len(chunk)
            
            # Show progress
            progress = (bytes_sent / len(firmware)) * 100
            print(f"\rProgress: {progress:.1f}% ({bytes_sent}/{len(firmware)} bytes)", end='')
        
        print("\nFirmware uploaded successfully!")
        
        # Send end command
        sock.send(b'\x02')  # OTA end command
        
        # Wait for response
        response = sock.recv(1)
        if response == b'\x00':
            print("OTA update successful!")
            return True
        else:
            print(f"OTA update failed with response: {response}")
            return False
            
    except Exception as e:
        print(f"OTA upload failed: {e}")
        return False
    finally:
        if 'sock' in locals():
            sock.close()

if __name__ == "__main__":
    host = "192.168.2.138"  # Use IP since hostname resolution may be unstable
    port = 3232  # Arduino OTA default port
    firmware_path = ".pio/build/control_loop/firmware.bin"
    
    print(f"Attempting OTA upload to {host}:{port}")
    success = esp32_ota_upload(host, port, firmware_path)
    sys.exit(0 if success else 1)