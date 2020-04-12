#! /bin/sh


case "$1" in
	start)
		echo "Starting sensor"
		start-stop-daemon -S -n sensor -a /usr/bin/sensor
		;;
	stop)
		echo "Stopping sensor"
		start-stop-daemon -K -n sensor
		;;
	*)
		echo "Usage: $0 {start|stop}"
	exit 1
esac

exit 0