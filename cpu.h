#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "defs.h"

extern void cpu_init();
extern void cpu_exit();
extern void cpu_run();
extern void cpu_run_once();
static void print_cpu();

typedef struct{
    u8 A;
    u8 B;
    u8 C;
    u8 D;
    u8 E;
    u8 H;
    u8 L;
    u8 F;//flags
    u32 PC;
    u32 SP;

    int interrupt_master_enable;
    unsigned long cpu_time;
    int cpu_halt;
    int cpu_stop;
    int cpu_exit_loop;
    int PC_skip; // HALT bug
    int interrupt_skip; // Doesn't jump to interrupt vector
    unsigned long cycle_counter;
    unsigned long jump_taken;
}Cpu;

#endif
