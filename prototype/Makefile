CFLAGS = -c -Wall -g
LDFLAGS = 
 
SOURCES = validity90/validity90.c validity90/utils.c
OBJECTS = $(SOURCES:.c=.o)
 
EXECUTABLE = prototype
 
LIBS = nss openssl libusb-1.0 libpng glib-2.0
CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS)) -lgcrypt

all: $(SOURCES) $(EXECUTABLE)
 
$(EXECUTABLE): $(OBJECTS) main.o
	$(CC) $(OBJECTS) main.o -o $@ $(LDFLAGS)
 
main.o: main.c
	$(CC) $(CFLAGS) -w $< -o $@

.o:
	$(CC) $(CFLAGS) $< -o $@

permissions:
	sudo chmod a+r /sys/class/dmi/id/product_serial
	lsusb -d 138a: | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $2 "/" $3 }' | xargs sudo chmod a+rwx 
 
clean:
	rm -rf $(OBJECTS) $(EXECUTABLE) main.o

