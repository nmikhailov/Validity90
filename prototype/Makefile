CFLAGS = -c -Wall -g -DG_LOG_DOMAIN=\"Validity90\"
LDFLAGS = 
 
SOURCES = validity90/utils.c validity90/validity90.c
SOURCES_GTEST = test/gtest.c test/rsp6-test.c test/utils-test.c

HEADERS = constants.h validity90/validity90.h

OBJECTS = $(SOURCES:.c=.o)
OBJECTS_GTEST = $(SOURCES_GTEST:.c=.o)
 
EXECUTABLE = prototype
 
LIBS = nss openssl libusb-1.0 libpng glib-2.0

CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS)) -lgcrypt

all: $(EXECUTABLE)
 
$(EXECUTABLE): $(OBJECTS) main.o
	$(CC) $(OBJECTS) main.o -o $@ $(LDFLAGS)
 
main.o: main.c $(HEADERS)
	$(CC) $(CFLAGS) -w $< -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@
 
test/%.o: test/%.c $(HEADERS)
	$(CC) $(CFLAGS) -I. $< -o $@

test/gtest: $(OBJECTS) $(OBJECTS_GTEST)
	$(CC) $(OBJECTS) $(OBJECTS_GTEST) -o $@ $(LDFLAGS)

test: test/gtest
	gtester -k --verbose test/gtest

permissions:
	sudo chmod a+r /sys/class/dmi/id/product_serial
	lsusb -d 138a: | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs sudo chmod a+rw
	lsusb -d 06cb:0081 | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs sudo chmod a+rw

clean:
	rm -f $(OBJECTS) $(OBJECTS_GTEST) $(EXECUTABLE) main.o test/gtest

.PHONY: all permissions test clean