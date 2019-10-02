#include <stdlib.h>

#include "timer.h"
#include "mem.h"

Timer *timer;

void timer_init() {
    timer = malloc(sizeof(Timer));

    timer->clock.main = 0;
    timer->clock.sub = 0;
    timer->clock.div = 0;

    timer->reg.div = 0;
    timer->reg.tima = 0;
    timer->reg.tma = 0;
    timer->reg.tac = 0;
}

void timer_step() {
    if(memory->debug)
	printf("%X\n", timer->reg.tima++)
    timer->reg.tima++;
    timer->clock.main = 0;

    if (timer->reg.tima > 0xFF) {
	timer->reg.tima = timer->reg.tma;
	// flag interrupt
	memory->interrupt_flags |= 0x04;
    }
}

void timer_check() {
    int threshold;

    if(timer->reg.tac & 0x04)
    {
	switch(timer->reg.tac & 0x03)
	{
	case 0: threshold = 64; break; // 4KHZ
	case 1: threshold = 1; break; // 256KHZ
	case 2: threshold = 4; break; // 64KHZ
	case 3: threshold = 16; break; //16KHZ
	default: threshold = 1; break;
	}
	if(timer->clock.main >= threshold) {
	    timer_step();
	    timer->clock.main -= threshold;
	}
    }
}

void timer_tick(int time) {
    timer->clock.sub += time;

    // if bit 2 is set the timer is enabled
    if(timer->reg.tac & 0x04) {

	while(timer->clock.sub >= 4) {
	    timer->clock.main++;
	    timer->clock.sub -= 4;

	    timer->clock.div++;
	    if(timer->clock.div == 16)
	    {
		timer->reg.div = (timer->reg.div + 1) & 0xFF;
		timer->clock.div = 0;
	    }
	}
	timer_check();
    }
}

u8 timer_read_byte(u16 address) {
    switch(address)
    {
    case 0xFF04: return timer->reg.div;
    case 0xFF05: return timer->reg.tima;
    case 0xFF06: return timer->reg.tma;
    case 0xFF07: return timer->reg.tac;
    default:
	return 0;
    }
}
void timer_write_byte(u16 address, u8 value) {
    switch(address)
    {
    case 0xFF04: timer->reg.div = 0; break;
    case 0xFF05: timer->reg.tima = value; break;
    case 0xFF06: timer->reg.tma = value; break;
    case 0xFF07: timer->reg.tac = value & 0x07; break;
    }
}
