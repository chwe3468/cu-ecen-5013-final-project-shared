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

TARGET = sensor serversocket
OBJECTS = Dhruva/platform_testing/sensor.o Sam/server/serversocket.o

all: sensor serversocket
default: sensor serversocket

sensor: Dhruva/platform_testing/sensor.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

serversocket: Sam/server/serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	-rm -rf *.o $(OBJECTS)
	-rm -rf $(TARGET)
