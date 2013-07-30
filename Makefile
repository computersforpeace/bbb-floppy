CC := $(CROSS)gcc
CFLAGS := -Wall -g
LDLIBS := -lm -lrt

OBJECTS := floppy.o move-ctrl.o

all: floppy

floppy: $(OBJECTS)

floppy.o: move-ctrl.h

move-ctrl.o: move-ctrl.h

clean:
	rm -f *.o floppy
