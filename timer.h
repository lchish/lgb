#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_check();
void timer_init();
void timer_tick(int);
u8 timer_read_byte(u16);
void timer_write_byte(u16, u8);

typedef struct{
    unsigned int main;
    unsigned int sub;
    unsigned int div;
} TimerClock;

typedef struct{
    u8 div; // divider register
    unsigned int tima; //timer counter
    u8 tma; // timer modulo
    u8 tac; // timer control
} TimerRegister;

typedef struct{
    TimerClock clock;
    TimerRegister reg;
} Timer;

extern Timer *timer;

#endif
