TOP_DIR = /media/user1/V3SSDK_BUILDROOT/v3ssdk/buildroot/out/
CROSS = $(TOP_DIR)host/bin/arm-buildroot-linux-gnueabihf-

CC = $(CROSS)gcc
LIBMODBUS_PATH=modbus

sedona:
	$(CC) -D__UNIX__ *.c -g -Wall -I$(LIBMODBUS_PATH)/ -L$(LIBMODBUS_PATH)/ -lmodbus -lpthread -lm -o svm
clean:
	rm svm
