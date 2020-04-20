# inspired by http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/
# Author: Dhruva Koley

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc # CC implicit keyword to add CROSS_COMPILE keyword before gcc if not empty
endif
ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror # error checking flags
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

TARGET = sensor inotify_test serversocket fb_write
OBJECTS = Dhruva/platform_testing/sensor.o Dhruva/platform_testing/inotify_test.o Sam/server/serversocket.o Sam/fbwrite/fbwrite.o

all: sensor inotify_test serversocket fbwrite
default: sensor inotify_test serversocket fbwrite

sensor: Dhruva/platform_testing/sensor.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

inotify_test: Dhruva/platform_testing/inotify_test.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

serversocket: Sam/server/serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

fbwrite: Sam/fbwrite/fbwrite.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	-rm -rf *.o $(OBJECTS)
	-rm -rf $(TARGET)
