CFLAGS = -c -Wall -g -DG_LOG_DOMAIN=\"Validity90\"
LDFLAGS = 
 
SOURCES = validity90/utils.c validity90/validity90.c
SOURCES_GTEST = test/rsp6-test.c test/utils-test.c

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
 
test/gtest.out.c: $(SOURCES_GTEST) test/gtest.c
	cp test/gtest.c $@
	awk '/@TEST_DEF/{ getline; print $$1 " " $$2 ";" }' $(SOURCES_GTEST) > $@.1
	awk '/@TEST_DEF/{ name=$$3; getline; print "g_test_add_func(\"" name "\", " gensub(/[^[A-Za-z0-9_]/, "", "g", $$2) ");" }' $(SOURCES_GTEST) > $@.2
	
	sed -i '/@TEST_INSERT_DEFINITION@/ r $@.1' $@
	sed -i '/@TEST_INSERT_DECLARATION@/ r $@.2' $@
	rm $@.1 $@.2

test/%.o: test/%.c $(HEADERS)
	$(CC) -Wno-unused-variable $(CFLAGS) -I. $< -o $@

test/gtest: $(OBJECTS) $(OBJECTS_GTEST) test/gtest.out.o
	$(CC) $(OBJECTS) $(OBJECTS_GTEST) test/gtest.out.o -o $@ $(LDFLAGS)

test: test/gtest
	gtester -k --verbose test/gtest

permissions:
	sudo chmod a+r /sys/class/dmi/id/product_serial
	lsusb -d 138a: | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs -r sudo chmod a+rw
	lsusb -d 06cb:0081 | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs -r sudo chmod a+rw
	lsusb -d 06cb:009a | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs -r sudo chmod a+rw

clean:
	rm -f $(OBJECTS) $(OBJECTS_GTEST) $(EXECUTABLE) main.o test/gtest test/gtest.out.*

.PHONY: all permissions test clean
