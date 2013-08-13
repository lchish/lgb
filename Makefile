CFLAGS = -Wall -g -lSDL2 -fprofile-arcs -ftest-coverage
CFILES = cpu.c mem.c gpu.c main.c display.c cpu_timings.c
HFILES=$(CFILES:.c=.h)
OBJECTS=$(CFILES:.c=.o)
EXECUTABLE=lgb
COMPILER=gcc 

all: $(CFILES) $(EXECUTABLE)
$(EXECUTABLE): $(OBJECTS)
	$(COMPILER) $(CFLAGS) $(OBJECTS) -o $@
clean:
	rm -rf *.o $(EXECUTABLE)
