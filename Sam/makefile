#Makefile for assignment 6
#Author: Sam Solondz

#If CROSS_COMPILE is not defined, just do gcc
ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

#compiler flags:
#	-Wall	turns on most compiler warnings
ifeq ($(CFLAGS),)
	CFLAGS = -g #-Wall -Werror
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt -lgcc_s
endif

TARGET = serversocket
OBJECTS = server/serversocket.o

all: serversocket
default: serversocket

serversocket: server/serversocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)


#%.o: %.c
#	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	-rm -rf *.o $(OBJECTS)
	-rm -rf $(TARGET)
