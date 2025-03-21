from ultralytics import YOLO
import cv2
import numpy as np
import paho.mqtt.client as mqtt
import json

# Define the broker address and port
broker = "192.168.2.2"  # Replace with your broker's address
port = 1883  # Default MQTT port

# ESP ID
PAR01="b4328a0a8ab4"
JCA01="38504720f540"

# Define the topic and message
topic = JCA01+"/control"

# Define the payload 
payload_dict = { "motor": 0, "direction":0}
motor_value = 0
direction_value = 0

# Create an MQTT client instance
client = mqtt.Client()

# Connect to the broker
client.connect(broker, port)

# Load the YOLOv8 model
model = YOLO('yolov8n.pt')

# Define the class IDs you want to detect
# bottle (39), cup (41), apple (47), cell phone (67) and toothbrush (79)
target_classes = [39,41,47,67,79]

forward_class = [39,41]
stop_class = [47,67,79]

# Initialize the webcam
cap = cv2.VideoCapture(0)

while True:
    # Read a frame from the webcam
    ret, frame = cap.read()
    if not ret:
        break

    # Run YOLOv8 inference on the frame
    results = model(frame)

    # Process the results
    for result in results:
        boxes = result.boxes  # Boxes object for bbox outputs
        for box in boxes:
            # Get the class ID
            cls = int(box.cls[0])
            
            # Check if the detected object is in our target classes
            if cls in target_classes:
                if cls in forward_class:
                    motor_value=100
                if cls in stop_class:
                    motor_value=0
                # Get the bounding box coordinates
                x1, y1, x2, y2 = box.xyxy[0]
                x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)

                # Draw bounding box
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

                # Add label
                conf = float(box.conf)
                label = f"{model.names[cls]} {conf:.2f}"
                cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)

                payload_dict["motor"]=motor_value
                payload_dict["direction"]=direction_value
       #         payload_dict["topic"]=topic
                result = client.publish(topic, json.dumps(payload_dict))

    # Display the frame
    cv2.imshow("Object Detection", frame)

    # Break the loop if 'q' is pressed
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# Release the webcam and close windows
cap.release()
cv2.destroyAllWindows()


