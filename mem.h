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
    unsigned int rom_bank;
    unsigned int ram_bank;
    unsigned int ram_on;
    unsigned int mode;
} MemoryBankController;

typedef struct{
    u8 rom[0x10000]; // 64KB
    u8 vram[0x2000];
    u8 eram[0x2000];
    u8 wram[0x2000];
    u8 oam[0xA0];
    u8 zram[0x80];
    u8 interrupt_enable;
    u8 interrupt_flags;
    int in_bios;
    int cart_type;
    u16 rom_offset;
    u16 ram_offset;
    MemoryBankController memory_bank_controllers[3];
}Memory;

extern Memory *memory;

#endif
