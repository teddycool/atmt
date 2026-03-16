Create a professional technical overview in landscape A3 format showing the hardware setup of an autonomous toy truck racing and training system. The diagram should include:

Left section  - the toy-trucks:
2 toy-trucks, one black and one orange. use the provided image for both but change color on the body for one and place them side by side but with a space between.
The trucks use the local wifi to communicate with the backend via MQTT
Each ot them use a control-loop for sending sensordata and receiving control-commands and updates


Center section  -  network infrastructure:
- 1 Teltonika RUT240 4G router, conncted to internet via 4G and creating a local wifi network 
- 1 TP-Link TL-SG205E gigabit switch connected

There are two 2 development laptops connected to the WiFi with vscode on the screens


Right section  -  the backend
1 Rasberry pi 5 running MQTT broker and SQL server 
1 Rasberry pi 5 with a Hailo AI hat running machine-learning software
1 nVidia jetson Nano running machine-learning software
1 Rasberry pi 3 running a webserver with a dashboard showing telemetry from the trucks
All of these connected to the LAN create by the gigabit switch