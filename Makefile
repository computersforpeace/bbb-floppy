CC := $(CROSS)g++
CPPFLAGS := -Wall -Wextra -g -MMD
LDLIBS := -lm -lrt -lpthread

OBJECTS := floppy.o move-ctrl.o

all: tone

floppy: $(OBJECTS)

tone: move-ctrl.o tone.o

-include $(OBJECTS:%=%.d)

clean:
	rm -f *.o *.d floppy

tags::
	ctags -R
