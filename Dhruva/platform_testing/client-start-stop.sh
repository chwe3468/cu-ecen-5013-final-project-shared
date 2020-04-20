#! /bin/sh

case "$1" in
	start)
		echo "Starting client"
		start-stop-daemon -S -n client1 -a /usr/bin/client1
		;;
	stop)
		echo "Stopping client"
		start-stop-daemon -K -n client1
		;;
	*)
		echo "Usage: $0 {start|stop}"
	exit 1
esac

exit 0