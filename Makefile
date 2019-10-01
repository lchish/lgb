CFLAGS := -Wall -g
LDFLAGS := -lSDL2
CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

SOURCES = cpu.c mem.c gpu.c main.c display.c cpu_timings.c timer-new.c
HFILES=$(CFILES:.c=.h)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=lgb
CC=gcc

all: $(OBJECTS) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
%: %.o
	$(CC) -o $@ $< $(CFLAGS)
clean:
	-rm -f $(EXECUTABLE) $(OBJECTS)
