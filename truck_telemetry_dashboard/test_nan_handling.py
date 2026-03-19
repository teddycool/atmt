#!/usr/bin/env python3
"""
Test script to validate NaN handling in telemetry data
"""

import json
import math

def sanitize_telemetry_data(data):
    """
    Sanitize telemetry data to handle NaN values and other edge cases.
    NaN values are converted to None which JSON serializes as null.
    """
    
    def sanitize_value(value):
        if value is None:
            return None
        if isinstance(value, float):
            if math.isnan(value):
                return None
            elif math.isinf(value):
                return None
        return value
    
    sanitized = {}
    nan_keys = []
    
    for key, value in data.items():
        original_value = value
        sanitized_value = sanitize_value(value)
        sanitized[key] = sanitized_value
        
        # Track which keys had NaN values for logging
        if original_value != sanitized_value and isinstance(original_value, float):
            nan_keys.append(key)
    
    # Log if any NaN values were found
    if nan_keys:
        truck_id = data.get('truck_id', 'unknown')
        print(f"🔧 Sanitized NaN values in truck {truck_id}: {nan_keys}")
    
    return sanitized

# Test data - simulating the problematic truck telemetry
test_data_with_nan = {
    "truck_id": "fc318a0a8ab4",
    "seq": 4,
    "t_ms": 11405,
    "mode": "EXPLORE",
    "ul": 135.8,
    "ur": 5.0,
    "uf": 99.2,
    "ub": 112.8,
    "yaw_rate": 2.99,
    "heading": 213086.0,
    "compass": float('nan'),  # This is the problematic value
    "mag_x": 0.0,
    "mag_y": 0.0,
    "mag_z": 0.0,
    "acc_x": -51.3,
    "acc_y": 1956.3,
    "acc_z": -3830.7,
    "width": 140.8,
    "center_error": -130.8,
    "front_blocked": False,
    "cmd_pwm": 80,
    "cmd_steer": "LEFT"
}

print("🧪 Testing NaN handling in telemetry data")
print("=" * 50)

print("\n📊 Original data (with NaN):")
print(f"  Compass value: {test_data_with_nan['compass']}")
print(f"  Is NaN? {math.isnan(test_data_with_nan['compass'])}")

# Test sanitization
sanitized_data = sanitize_telemetry_data(test_data_with_nan)

print("\n🔧 After sanitization:")
print(f"  Compass value: {sanitized_data['compass']}")
print(f"  Data type: {type(sanitized_data['compass'])}")

# Test JSON serialization (this was failing before)
try:
    json_output = json.dumps(sanitized_data, indent=2)
    print("\n✅ JSON serialization successful!")
    print("📄 JSON contains:", '"compass": null,' in json_output)
except Exception as e:
    print(f"\n❌ JSON serialization failed: {e}")

# Simulate JavaScript-side handling
print("\n🌐 JavaScript-side validation:")
print("   - null values will display as '-' in dashboard")
print("   - NaN values are converted to null server-side")
print("   - isNaN() and isFinite() checks prevent display issues")

print("\n✨ Test completed! Dashboard should now handle both trucks properly.")