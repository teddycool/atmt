To run the detection model to steer the car via mqtt do the following
1) Create a virtual envirnoment
2) python3 -m venv yolo_env
3) pip install -r requirements
4) python3 detection_script.py

Make sure that the ESP32 ID is added in the remote_image_detection and being used a a
topic

You can also run the remote.sh bash script to steer the car with A,D,S,W
keystrokes
