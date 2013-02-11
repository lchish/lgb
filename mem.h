#ifndef MEM_H
#define MEM_H
#include <stdio.h>

#include "defs.h"
#include "types.h"

void mem_init();
void load_rom(FILE* f);
u8 get_mem(u16 address);
u16 get_mem_16(u16 address);
void set_mem(u16 address,u8 value);
void set_mem_16(u16 address,u16 value);

typedef struct{
    u8 rom[0x8000];
    u8 vram[0x2000];
    u8 eram[0x2000];
    u8 wram[0x2000];
    u8 zram[0x80];
    u8 interrupt_enable;
    u8 interrupt_flags;
    int in_bios;
}Memory;

extern Memory *memory;

#endif
