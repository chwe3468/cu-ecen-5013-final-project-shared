#!/bin/sh
# Sensor script to call vcgencmd measure_temp and strip output and save to a log file
# Author: Dhruva Koley

LOGDIR="/var/tmp/log"
LOGFILE="${LOGDIR}/log.txt"

if !([ -d "${LOGDIR}" ])
then
	# build log directory if it doesn't exist
	cd "/var/tmp/"
	mkdir -p "$LOGDIR"
fi

string="temp=00.0'C"
string=$(vcgencmd "measure_temp")
# strip the front and the end of the string to isolate the temperature
string2=${string#"temp="}
string2=${string2%"'C"}

echo $string2 >> "${LOGFILE}"
echo $string2

return 0