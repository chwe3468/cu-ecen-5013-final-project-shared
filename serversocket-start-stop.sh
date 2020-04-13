#!/bin/sh

#/usr/bin/aesdsocket
case "$1" in
	start)
		#Start the daemon
		echo "Starting server"
		start-stop-daemon --start -n -serversocket -a /usr/bin/serversocket -- '-d'
		;;

	stop)
		#Send SIGTERM handler to daemon
		echo "Stopping server"
		start-stop-daemon --stop -n serversocket
		;;

	*)
		echo "Usage: $0 {start | stop}"
		exit 1
esac

	
