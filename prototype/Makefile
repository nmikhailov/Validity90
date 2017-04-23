CFLAGS = -c
LDFLAGS =
 
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)
 
EXECUTABLE = prototype
 
LIBS = nss openssl libusb-1.0 libpng
CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

all: $(SOURCES) $(EXECUTABLE)
 
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
 
.o:
	$(CC) $(CFLAGS) $< -o $@
 
clean:
	rm $(OBJECTS) $(EXECUTABLE)