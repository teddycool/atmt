#!/bin/bash

VEHICLE_ID="38504720f540"
HOST=192.168.2.2
MQTT="mosquitto_pub"
STATE_MOTOR=0
STATE_DIRECTION=0
#VEHICLE_ID="b4328a0a8ab4"
steer_right=10
steer_left=-${steer_right}


key_function() {
    case $1 in
        q|Q) echo "Quitting..."; exit ;;
        a|A)  echo "You pressed A left"; 
              if [ ${STATE_DIRECTION} -ge 0 ]
              then
                STATE_DIRECTION=${steer_left}
              elif [ ${STATE_DIRECTION} -gt -100 ]
              then
                let "STATE_DIRECTION += steer_left"
              fi
              ;;
        s|S) echo "You pressed S stop/reverse"; 
              if [ $STATE_MOTOR -eq "0" ]
              then 
                  STATE_MOTOR=-100
              elif [ $STATE_MOTOR -gt "0" ]
              then
                  STATE_MOTOR=0
              fi
              ;;
        w|W) echo "You pressed W forward/faster"; 
              if [ $STATE_MOTOR -eq "0" ]
              then
                STATE_MOTOR=100
              elif [ $STATE_MOTOR -lt "0" ]
              then
                STATE_MOTOR=0
              fi
              STATE_DIRECTION=0
              ;;

        d|D) echo "You pressed C right"; 
              if [ ${STATE_DIRECTION} -le 0 ]
              then
                STATE_DIRECTION=${steer_right}
              elif [ ${STATE_DIRECTION} -lt 100 ]
              then
                let "STATE_DIRECTION += steer_right"
              fi
              ;;
        *) echo "You pressed: $1" ;;
    esac
    echo mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/control" -m "{\"motor\":${STATE_MOTOR}, \"direction\":${STATE_DIRECTION}}"
    mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/control" -m "{\"motor\":${STATE_MOTOR}, \"direction\":${STATE_DIRECTION}}"
}

echo "Press 'a' 's' 'd' 'w' for steering; (q to quit):"
echo "Choose the car you want to steer "




while true; do
    read -n 1 -s key
    key_function "$key"
done
