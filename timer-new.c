#include <stdlib.h>

#include "timer-new.h"
#include "mem.h"

Timer *timer;

void timer_init() {
    timer = malloc(sizeof(Timer));
    timer->div = 0;
    timer->tima = 0;
    timer->timo = 0;
    timer->tac = 0;
}

void timer_step() {
    timer->tima++;
    if (timer->tima > 0xFF) {
	timer->tima = timer->timo;
	// flag interrupt
	memory->interrupt_flags |= 0x04;
    }
}

void timer_check() {
    int threshold = 0;

    switch(timer->tac & 0x03)
    {
    case 0: threshold = 1024; break; // 4KHZ
    case 1: threshold = 16; break; // 256KHZ
    case 3: threshold = 64; break; // 64KHZ
    case 2: threshold = 256; break; //16KHZ
    }
    while(timer->_div >= threshold) {
	timer->_div -= threshold;
	timer_step();
    }
}

void timer_tick(int time) {
    timer->div = (timer->div + time) & 0xFFFF;
    timer->_div = (timer->_div + time) & 0xFFFF;
    // if bit 2 is set the timer is enabled
    if(timer->tac & 0x04) {
	timer_check();
    }
}

u8 timer_read_byte(u16 address) {
    printf("read %X %X\n", address, timer->tima);
    switch(address)
    {
    case 0xFF04: return timer->div >> 8;
    case 0xFF05: return timer->tima;
    case 0xFF06: return timer->timo;
    case 0xFF07: return timer->tac;
    default:
	return 0;
    }
}

void timer_write_byte(u16 address, u8 value) {
    printf("write %X %X\n", address, value);
    switch(address)
    {
    case 0xFF04: timer->div = 0; timer->_div = 0; break;
    case 0xFF05: timer->tima = value; break;
    case 0xFF06: timer->timo = value; break;
    case 0xFF07: timer->tac = value & 0x07; break;
    }
}
