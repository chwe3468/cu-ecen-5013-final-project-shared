# inspired by http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/
# Author: Dhruva Koley

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc # CC implicit keyword to add CROSS_COMPILE keyword before gcc if not empty
endif
ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror # error checking flags
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt -lgcc_s
endif

SRCS := $(wildcard *.c)	# set all files with .c extensions as SRCS
BINS := $(SRCS:%.c=%) # set name of all output binaries with same name as .c files in SRCS

all: sensor client1 serversocket

default: sensor client1

client1: client1.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

sensor: sensor.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

serversocket: serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	rm -rvf *.o ${BINS}	# recursively force remove all .o files and all binaries in BINS

