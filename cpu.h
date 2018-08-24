#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "defs.h"

extern void cpu_init();
extern void cpu_exit();
extern void cpu_run();
extern void cpu_run_once();
static void print_cpu();
#endif
