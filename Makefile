CC := $(CROSS)gcc
CPPFLAGS := -Wall -Wextra -g -MMD
LDLIBS := -lm -lrt

OBJECTS := floppy.o move-ctrl.o

all: floppy

floppy: $(OBJECTS)

-include $(OBJECTS:%=%.d)

clean:
	rm -f *.o *.d floppy

tags::
	ctags -R
