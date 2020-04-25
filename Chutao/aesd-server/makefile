# Name: makefile
# Description: makefile for project 1 for ECEN 5013 AESD
# Toolchain: gcc compiler version 7.4.0


CROSS_COMPILE =# Cross compile option for arm-unknown-linux-gnueabi-

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(LD),)
	LD = ld  # Linker
endif

ifeq ($(CFLAGS),)
	CCFLAGS = -g -Wall -Werror
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

DEPS = queue.h # header files
OBJ = aesd_server.o

TARGET = aesd_server


all: $(TARGET)
	
aesd_server: $(OBJ)
	$(CC) $(CFLAGS) $@.o -o $@ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<  

clean:
	-rm -f aesd_server *.o *.s
