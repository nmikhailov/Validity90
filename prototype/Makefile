CFLAGS = -c -Wall -g
LDFLAGS = 
 
SOURCES = main.c validity90/validity90.c
OBJECTS = $(SOURCES:.c=.o)
 
EXECUTABLE = prototype
 
LIBS = nss openssl libusb-1.0 libpng glib-2.0
CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

all: $(SOURCES) $(EXECUTABLE)
 
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
 
.o:
	$(CC) $(CFLAGS) $< -o $@
 
clean:
	rm $(OBJECTS) $(EXECUTABLE)
