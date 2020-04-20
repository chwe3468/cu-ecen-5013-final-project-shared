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

TARGET = sensor client1 serversocket fb_write
OBJECTS = Dhruva/platform_testing/sensor.o Dhruva/platform_testing/client1.o Sam/server/serversocket.o Sam/fbwrite/fbwrite.o

all: sensor client1 serversocket fbwrite
default: sensor client1 serversocket fbwrite

sensor: Dhruva/platform_testing/sensor.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

client1: Dhruva/platform_testing/client1.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

serversocket: Sam/server/serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

fbwrite: Sam/fbwrite/fbwrite.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	-rm -rf *.o $(OBJECTS)
	-rm -rf $(TARGET)
