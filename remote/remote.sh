#!/bin/bash

# default values for the mqtt broker and vehicle id
HOST=192.168.2.2
VEHICLE_ID_JCA01="38504720f540"
VEHICLE_ID_PAR01="b4328a0a8ab4"
VEHICLE_ID=${VEHICLE_ID_PAR01}

# some nice lines for the console output
echo "==============================================================================================="

# getopts to give mqtt broker and vehicle id as input
# help to know the usage of the script
while getopts 's:i:h' opt;do
  case "$opt" in
    s) HOST=${OPTARG}; echo "#  MQTT broker set to '${HOST}'" ;;
    i) VEHICLE_ID=${OPTARG}; echo "#   Vehicle ID set to '${VEHICLE_ID}'";;
    h) echo "Usage: $(basename $0) [-s <IPv4 of mqtt broker>] [-i VEHICLE ID]"; 
      echo "List of stored VEHICLE IDS '${VEHICLE_ID_JCA01}' and '${VEHICLE_ID_PAR01}'";
      exit 0; ;;
    ?) echo "Invalid option: -$OPTART" ; exit 1; ;;
  esac
done
#shift "$(($OPTIND -1))"

# default mqtt client and initial state of variables
MQTT="mosquitto_pub"
STATE_MOTOR=0
STATE_DIRECTION=0
steer_right=10
steer_left=-${steer_right}

# Check if MQTT client exits if not exit
if command -v ${MQTT} &> /dev/null ; 
then
  echo "#  ${MQTT} client found"
else
  echo "${MQTT} client not found, please install the client first"
  exit 1;
fi

# Display the configuration to be sued in the script before starting
echo "==============================================================================================="
echo "# Using MQTT broker '${HOST}', VEHICLE_ID '${VEHICLE_ID}' and MQTT Client '${MQTT}' #"
echo "==============================================================================================="

# function that will be used to detect the keys to steer the vehicle
key_function() {
  trigger=false
    case $1 in
        q|Q) echo "Quitting..."; exit ;;
        a|A)  echo "You pressed A left"; trigger=true
              if [ ${STATE_DIRECTION} -ge 0 ]
              then
                STATE_DIRECTION=${steer_left}
              elif [ ${STATE_DIRECTION} -gt -100 ]
              then
                let "STATE_DIRECTION += steer_left"
              fi
              ;;
        s|S) echo "You pressed S stop/reverse"; trigger=true
              if [ $STATE_MOTOR -eq "0" ]
              then 
                  STATE_MOTOR=-100
              elif [ $STATE_MOTOR -gt "0" ]
              then
                  STATE_MOTOR=0
              fi
              ;;
        w|W) echo "You pressed W forward/faster"; trigger=true
              if [ $STATE_MOTOR -eq "0" ]
              then
                STATE_MOTOR=100
              elif [ $STATE_MOTOR -lt "0" ]
              then
                STATE_MOTOR=0
              fi
              STATE_DIRECTION=0
              ;;

        d|D) echo "You pressed C right"; trigger=true
              if [ ${STATE_DIRECTION} -le 0 ]
              then
                STATE_DIRECTION=${steer_right}
              elif [ ${STATE_DIRECTION} -lt 100 ]
              then
                let "STATE_DIRECTION += steer_right"
              fi
              ;;
    esac
    
    if  $trigger 
    then
      echo mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/control" -m "{\"motor\":${STATE_MOTOR}, \"direction\":${STATE_DIRECTION}}"
      mosquitto_pub -h ${HOST} -t "${VEHICLE_ID}/control" -m "{\"motor\":${STATE_MOTOR}, \"direction\":${STATE_DIRECTION}}"
    fi
    trigger=false
}

# description of how to use the script to steer and how to quit
echo "Press 'a' 's' 'd' 'w' for steering; (q to quit):"

# while loop to start the remote control
while true; do
    read -n 1 -s key
    key_function "$key"
done
