#!/bin/sh
# Sensor script to call vcgencmd measure_temp and strip output and save to a log file
# Author: Dhruva Koley

LOGDIR="log"
LOGFILE="${LOGDIR}/log.txt"

# DIR="$( cd "$(dirname "$0")" ; pwd -P )"
DIR="/home/dhruva/aesd/finalproject/cu-ecen-5013-final-project-shared/Dhruva/native_testing/"

if !([ -d "${DIR}/${LOGDIR}" ])
then
	# build log directory if it doesn't exist
	cd ${DIR}
	mkdir -p "${DIR}/${LOGDIR}"
fi

string="temp=00.0'C"
# string=$(vgencmd "measure_temp")
# strip the front and the end of the string to isolate the temperature
string2=${string#"temp="}
string2=${string2%"'C"}
echo $string2 >> "${DIR}${LOGFILE}"
echo $string2
return 0

