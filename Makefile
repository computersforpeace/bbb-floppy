CC := gcc
LDLIBS := -lm -lrt

all: floppy

floppy: floppy.o
