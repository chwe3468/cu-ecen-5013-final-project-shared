#!/bin/sh

#/usr/bin/aesdsocket
case "$1" in
	start)
		#Start the daemon
		echo "Starting server"
		start-stop-daemon --start -n -aesdsocket -a /usr/bin/aesdsocket -- '-d'
		;;

	stop)
		#Send SIGTERM handler to daemon
		echo "Stopping server"
		start-stop-daemon --stop -n aesdsocket
		;;

	*)
		echo "Usage: $0 {start | stop}"
		exit 1
esac

	