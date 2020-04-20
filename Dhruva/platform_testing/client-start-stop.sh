#! /bin/sh

case "$1" in
	start)
		echo "Starting inotify test"
		/usr/bin/inotify_test
		;;
	stop)
		echo "Stopping inotify test"
		killall -s 9 inotify_test
		;;
	*)
		echo "Usage: $0 {start|stop}"
	exit 1
esac

exit 0