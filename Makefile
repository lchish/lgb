CFLAGS := -Wall -g
LFLAGS := -lSDL2
SOURCES = cpu.c mem.c gpu.c main.c display.c cpu_timings.c
HFILES=$(CFILES:.c=.h)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=lgb
CC=gcc

all: $(OBJECTS) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o $@
%: %.o
	$(CC) -o $@ $< $(CFLAGS)
clean:
	-rm -f $(EXECUTABLE) $(OBJECTS)
