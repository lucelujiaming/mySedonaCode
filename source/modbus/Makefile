TOP_DIR = /media/user1/V3SSDK_BUILDROOT/v3ssdk/buildroot/out/
CROSS = $(TOP_DIR)host/bin/arm-buildroot-linux-gnueabihf-

CC = $(CROSS)gcc
STRIP = $(CROSS_COMPILE)strip
CFLAGS = -std=gnu99 -g -Wall -O3

LIB_MODBUS_SRC_LIST = modbus.c modbus-data.c modbus-rtu.c modbus-tcp.c ../v3s_gpio_operation.c

all: libmodbus.so
	
clean:
	rm -rf *.o
	rm -rf libmodbus.so

libmodbus.so:
	$(CC) $(CFLAGS) -fPIC -shared $(LIB_MODBUS_SRC_LIST) -I. -I./ -o $@

