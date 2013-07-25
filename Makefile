CC := gcc
CFLAGS := -Wall
LDLIBS := -lm -lrt

all: floppy

floppy: floppy.o

clean:
	rm -f *.o floppy
