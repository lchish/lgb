#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_check();
void timer_init();
void timer_tick(const unsigned int);
u8 timer_read_byte(u16);
void timer_write_byte(u16, u8);

typedef struct{
    unsigned int div; // divider register
    unsigned int _div; // divider register

    unsigned int tima; //timer counter
    unsigned int timo; // timer modulo
    unsigned int tac; // timer control
} Timer;

extern Timer *timer;

#endif
