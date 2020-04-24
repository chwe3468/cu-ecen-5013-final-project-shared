#!/bin/sh
# Sensor script to call vcgencmd measure_temp and strip output and save to a log file
# Author: Dhruva Koley

LOGDIR="/var/tmp/log"
LOGFILE="${LOGDIR}/log.txt"

if !([ -d "${LOGDIR}" ])
then
	# build log directory and log file if it doesn't exist
	cd "/var/tmp/"
	mkdir -p "$LOGDIR"
	touch "${LOGFILE}"
fi
# build the log file if it doesn't exist
touch "${LOGFILE}"
# initialize the string
string="temp=00.0'C"
# poll the VideoCore GPU to get the CPU temperature
# returns a string "temp=xx.x`C"
string=$(vcgencmd "measure_temp")
# strip the beginning and the end of the string to isolate the temperature value
string2=${string#"temp="}
string2=${string2%"'C"} # because the front fell off
# append the processed string to the log file
echo $string2 >> "${LOGFILE}"

return 0
