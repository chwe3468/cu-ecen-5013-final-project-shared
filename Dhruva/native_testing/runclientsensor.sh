#!/bin/sh
# Sensor script to call vcgencmd measure_temp and strip output and save to a log file
# Author: Dhruva Koley

LOGDIR="log"
LOGFILE="${LOGDIR}/log.txt"

DIR="$( cd "$(dirname "$0")" ; pwd -P )"

./sensor
./client1

