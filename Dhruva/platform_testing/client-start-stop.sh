#! /bin/sh

case "$1" in
	start)
		echo "Starting client"
		start-stop-daemon -S -n inotify_test -a /usr/bin/inotify_test
		;;
	stop)
		echo "Stopping client"
		start-stop-daemon -K -n inotify_test
		;;
	*)
		echo "Usage: $0 {start|stop}"
	exit 1
esac

exit 0