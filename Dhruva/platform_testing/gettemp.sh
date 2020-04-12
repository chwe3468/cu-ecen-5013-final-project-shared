#!/bin/sh
# Sensor script to call vcgencmd measure_temp and strip output and save to a log file
# Author: Dhruva Koley

string="temp=00.0'C"
string=$(vcgencmd "measure_temp")
# strip the front and the end of the string to isolate the temperature
string2=${string#"temp="}
string2=${string2%"'C"}
echo $string2 >> "/var/tmp/log.txt"
echo $string2
return 0

