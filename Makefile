CC := $(CROSS)gcc
CFLAGS := -Wall -g
LDLIBS := -lm -lrt

all: floppy

floppy: floppy.o move-ctrl.o

floppy.o: move-ctrl.h

move-ctrl.o: move-ctrl.h

clean:
	rm -f *.o floppy
