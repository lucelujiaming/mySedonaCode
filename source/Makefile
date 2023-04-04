CROSS = armv5l-linux-

CC = $(CROSS)gcc
LIBMODBUS_PATH=modbus

sedona:
	$(CC) -D__UNIX__ *.c -Wall -O2 -I$(LIBMODBUS_PATH)/ -L$(LIBMODBUS_PATH)/ -lmodbus -lpthread -lm -o svm
clean:
	rm svm
