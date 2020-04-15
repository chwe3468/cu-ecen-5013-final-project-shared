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

TARGET = sensor serversocket fb_write
OBJECTS = Dhruva/platform_testing/sensor.o Sam/server/serversocket.o Sam/fb_test/fb_write.o

all: sensor serversocket fb_write
default: sensor serversocket fb_write

sensor: Dhruva/platform_testing/sensor.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

serversocket: Sam/server/serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

fb_write: Sam/fb_write/fb_test.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)
clean:
	-rm -rf *.o $(OBJECTS)
	-rm -rf $(TARGET)
