CFLAGS = -c -Wall -g -DG_LOG_DOMAIN=\"Validity90\"
LDFLAGS = 
 
SOURCES = 
SOURCES_GTEST = test/gtest.cpp test/rsp6-test.cpp test/utils-test.cpp

HEADERS = constants.h validity90/validity90.h

OBJECTS = $(SOURCES:.c=.o)
OBJECTS_GTEST = $(SOURCES_GTEST:.cpp=.o)
 
EXECUTABLE = prototype
 
LIBS = nss openssl libusb-1.0 libpng glib-2.0
LIBS_GTEST = gtest gmock
CFLAGS_GTEST = $(shell pkg-config --cflags $(LIBS_GTEST))
LDFLAGS_GTEST = $(shell pkg-config --libs $(LIBS_GTEST))

CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS)) -lgcrypt

all: $(EXECUTABLE)
 
$(EXECUTABLE): $(OBJECTS) main.o
	$(CC) $(OBJECTS) main.o -o $@ $(LDFLAGS)
 
main.o: main.c $(HEADERS)
	$(CC) $(CFLAGS) -w $< -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@
 
test/%.o: test/%.cpp $(HEADERS)
	$(CXX) $(CFLAGS) $(CFLAGS_GTEST) -I. $< -o $@

test/gtest: $(OBJECTS) $(OBJECTS_GTEST)
	$(CXX) $(OBJECTS) $(OBJECTS_GTEST) -o $@ $(LDFLAGS) $(LDFLAGS_GTEST)

test: test/gtest
	test/gtest

permissions:
	sudo chmod a+r /sys/class/dmi/id/product_serial
	lsusb -d 138a: | awk -F '[^0-9]+' '{ print "/dev/bus/usb/" $$2 "/" $$3 }' | xargs sudo chmod a+rw

clean:
	rm -f $(OBJECTS) $(OBJECTS_GTEST) $(EXECUTABLE) main.o test/gtest

.PHONY: all permissions test clean