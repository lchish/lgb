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
    u8 *rom;
    u8 vram[0x2000];
    u8 *eram; // 32KB
    u8 wram[0x2000];
    u8 oam[0xA0];
    u8 zram[0x80];
    u8 interrupt_enable;
    u8 interrupt_flags;
    int in_bios;
    int rom_banks;
    int ram_banks;
    u32 rom_offset;
    u32 ram_offset;
    int memory_bank_controller;
    MemoryBankController memory_bank_controllers[2];
    u32 debug;
}Memory;

extern Memory *memory;

#endif
