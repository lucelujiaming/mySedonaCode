CROSS_COMPILE=armv5l-linux-
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip
CFLAGS = -std=gnu99 -g -Wall -O3

LIB_MODBUS_SRC_LIST = modbus.c modbus-data.c modbus-rtu.c modbus-tcp.c

all: libmodbus.so
	
clean:
	rm -rf *.o
	rm -rf libmodbus.so

libmodbus.so:
	$(CC) $(CFLAGS) -fPIC -shared $(LIB_MODBUS_SRC_LIST) -I. -I./ -o $@
	$(STRIP) $@

