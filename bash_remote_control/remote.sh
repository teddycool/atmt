#!/bin/bash

VEHICLE_ID="38504720f540"
HOST=192.168.2.2
MQTT="mosquitto_pub"
STATE_MOTOR=0
#VEHICLE_ID="b4328a0a8ab4"


key_function() {
    case $1 in
        q|Q) echo "Quitting..."; exit ;;
        a|A)  echo "You pressed A left"; 
              echo "New STATE_LEFT = ${STATE_LEFT}"
              mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/steer" -m "left"
              ;;
        s|S) echo "You pressed S stop/reverse"; 
              if [ $STATE_MOTOR -eq "0" ]
              then 
                  STATE_MOTOR=-100
              elif [ $STATE_MOTOR -gt "0" ]
              then
                  STATE_MOTOR=0
              fi
                mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/motor" -m "${STATE_MOTOR}"  
                ;;
        w|W) echo "You pressed W forward/faster"; 
              if [ $STATE_MOTOR -eq "0" ]
              then
                STATE_MOTOR=100
              elif [ $STATE_MOTOR -lt "0" ]
              then
                STATE_MOTOR=0
              fi
              mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/motor" -m "${STATE_MOTOR}" 
              echo mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/motor" -m "${STATE_MOTOR}" 
              mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/steer" -m "stop"
              echo mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/steer" -m "stop"
                  ;;

        d|D) echo "You pressed C right"; 
              echo "New STATE_LEFT = ${STATE_RIGHT}"
              mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/steer" -m "right"
              ;;
        *) echo "You pressed: $1" ;;
    esac
}

echo "Press 'a' 's' 'd' 'w' for steering; (q to quit):"
echo "Choose the car you want to steer "




while true; do
    read -n 1 -s key
    key_function "$key"
done
