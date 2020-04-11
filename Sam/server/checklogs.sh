#!/bin/sh

echo "Syslog:"
tail -20 /var/log/syslog
echo "\n/var/tmp/aesdsocketdata:"
cat /var/tmp/aesdsocketdata
