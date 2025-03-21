from ultralytics import YOLO
import cv2
import numpy as np

# Load the YOLOv8 model
model = YOLO('yolov8n.pt')

# Define the class IDs you want to detect
# For example, person (0), car (2), and dog (16)
target_classes = [39,41,47,67,79]

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
                # Get the bounding box coordinates
                x1, y1, x2, y2 = box.xyxy[0]
                x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)

                # Draw bounding box
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

                # Add label
                conf = float(box.conf)
                label = f"{model.names[cls]} {conf:.2f}"
                cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)

    # Display the frame
    cv2.imshow("Object Detection", frame)

    # Break the loop if 'q' is pressed
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# Release the webcam and close windows
cap.release()
cv2.destroyAllWindows()


