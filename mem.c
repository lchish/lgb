#include <stdio.h>

#include "types.h"
#include "cpu.h"
#include "mem.h"
#include "defs.h"
#include "bios.h"
#include "gpu.h"
#include "display.h"

#define SYSTEM_JOYPAD_TYPE_REGISTER 0xFF00
#define SERIAL_TRANSFER_DATA 0xFF01
#define SIO_CONTROL 0xFF02
#define DIVIDER_REGISTER 0xFF04
#define TIMER_COUNTER 0xFF05
#define TIMER_MODULO 0xFF06
#define TIMER_CONTROL 0xFF07
#define INTERRUPT_FLAG 0xFF0F

#define SOUND_MODE_1_SWEEP_REGISTER 0xFF10
#define SOUND_MODE_1_LENGTH_PATTERN_REGISTER 0xFF11
#define SOUND_MODE_1_ENVELOPE_REGISTER 0xFF12
#define SOUND_MODE_1_FREQUENCY_LOW_REGISTER 0xFF13
#define SOUND_MODE_1_FREQUENCY_HIGH_REGISTER 0xFF14
#define SOUND_MODE_2_SOUND_LENGTH_REGISTER 0xFF16
#define SOUND_MODE_2_ENVELOPE_REGISTER 0xFF17
#define SOUND_MODE_2_FREQUENCY_LOW_REGISTER 0xFF18
#define SOUND_MODE_2_FREQUENCY_HIGH_REGISTER 0xFF19
#define SOUND_MODE_3_SOUND_ON_OFF_REGISTER 0xFF1A
#define SOUND_MODE_3_SOUND_LENGTH_REGISTER 0xFF1B
#define SOUND_MODE_3_OUTPUT_LEVEL_REGISTER 0xFF1C
#define SOUND_MODE_3_FREQUENCY_LOW_DATA_REGISTER 0xFF1D
#define SOUND_MODE_3_FREQUENCY_HIGH_DATA_REGISTER 0xFF1E

#define SOUND_MODE_4_SOUND_LENGTH_REGISTER 0xFF20
#define SOUND_MODE_4_ENVELOPE_REGISTER 0xFF21
#define SOUND_MODE_4_POLYNOMIAL_COUNTER_REGISTER 0xFF22
#define SOUND_MODE_4_COUNTER_REGISTER 0xFF23
#define SOUND_CHANNEL_CONTROL 0xFF24
#define SOUND_OUTPUT_SELECTION_TERMINAL 0xFF25
#define SOUND_ON_OFF 0xFF26

#define WAVE_PATTERN_RAM 0xFF30 //0xFF30-0xFF3F

#define LCD_CONTROL_REGISTER 0xFF40
#define LCD_STATUS_REGISTER 0xFF41
#define LCD_SCROLL_Y_REGISTER 0xFF42
#define LCD_SCROLL_X_REGISTER 0xFF43
#define CURRENT_FRAME_LINE 0xFF44
#define FRAME_LINE_COMPARE 0xFF45
#define DMA_TRANSFER 0xFF46
#define BACKGROUND_PALETTE_MEMORY 0xFF47
#define OBJECT_PALETTE0_MEMORY 0xFF48
#define OBJECT_PALETTE1_MEMORY 0xFF49
#define WINDOW_Y_POS 0xFF4A
#define WINDOW_X_POS 0xFF4B

#define DISABLE_BOOT_ROM 0xFF50
#define INTERRUPT_ENABLE 0xFFFF

Memory *memory;

void mem_init(){
    memory = malloc(sizeof(Memory));
    memory->in_bios=1;
}

void load_rom(FILE* f){
    int tmp,index=0;
    while((tmp = getc(f)) != EOF && index < 0x8000){
        memory->rom[index++] = tmp;
    }
    memory->in_bios = 1;//start off in the bios
}

//get and set the value at a memory address
u8 get_mem(u16 address){
    if(memory->in_bios){
        if(address < 0x100)
            return bios[address];
    }
    switch (address & 0xF000){
        //ROM 32KB
    case 0x0000: case 0x1000: case 0x2000: case 0x3000: case 0x4000:
    case 0x5000: case 0x6000: case 0x7000:
        return memory->rom[address];
        //Video Ram 2KB
    case 0x8000: case 0x9000:
        return memory->vram[address - 0x8000];
        //Switchable ram 2KB
    case 0xA000: case 0xB000:
        return memory->eram[address - 0xA000];
        //internal ram 2KB
    case 0xC000: case 0xD000:
        return memory->wram[address - 0xC000];
        //echo internal ram 2KB
    case 0xE000:
        return memory->wram[address - 0xE000];
    case 0xF000:
        switch(address & 0x0F00){
        case 0x000: case 0x100: case 0x200: case 0x300: case 0x400:
        case 0x500: case 0x600: case 0x700: case 0x800: case 0x900:
        case 0xA00: case 0xB00: case 0xC00: case 0xD00:
            return memory->wram[address - 0xE000];
        case 0xE00:
            if(address < 0xFEA0)
                return memory->oam[address - 0xFE00];
            else   
                printf("reading from empty ram address %X\n", address);
        case 0xF00:
            if(address == INTERRUPT_ENABLE){
                return memory->interrupt_enable;
            }
            if(address < 0xFF80){
                switch(address){
                case 0xFF00://get keys being pressed
                    return display_get_key();
                case LCD_CONTROL_REGISTER:
                    return get_lcd_control_register();
                case LCD_STATUS_REGISTER:
                    return gpu_get_status_register();
                case LCD_SCROLL_Y_REGISTER:
                    return get_scroll_y();
                case LCD_SCROLL_X_REGISTER://lcd scroll X register
                    return get_scroll_x();
                case CURRENT_FRAME_LINE://get the current frame line
                    return gpu_get_line();
                case BACKGROUND_PALETTE_MEMORY:
                    return gpu_get_palette(BACKGROUND_PALETTE);
                case OBJECT_PALETTE0_MEMORY:
                    return gpu_get_palette(OBJECT_PALETTE0);
                case OBJECT_PALETTE1_MEMORY:
                    return gpu_get_palette(OBJECT_PALETTE1);
                case INTERRUPT_FLAG:
                    return memory->interrupt_flags;
                default:
//printf("Reading from address: %x not handled\n",address);
                    return 0;
                }
            }
            if(address >= 0xFF80)
                return memory->zram[address - 0xFF80];
        }
    default:
        fprintf(stderr,"Reading from invalid memory address: %x \n",address);
        return 0;
    }
}

u16 get_mem_16(u16 address){
    return (get_mem(address) <<8) + get_mem(address+1);
}

void set_mem(u16 address,u8 value){
    switch(address & 0xF000){
        //ROM 32KB
    case 0x0000: case 0x1000: case 0x2000: case 0x3000: case 0x4000:
    case 0x5000: case 0x6000: case 0x7000:
        fprintf(stderr,"Rom memory cannot be written to\n");
        break;
        //VRAM 2KB
    case 0x8000: case 0x9000:
        memory->vram[address-0x8000] = value;
        if(address <= TILE_DATA_END)
            update_tile(address);
        return;
        //Swtichable RAM 2KB
    case 0xA000: case 0xB000:
        memory->eram[address-0xA000] = value;
        return;
        //Internal RAM 2KB
    case 0xC000: case 0xD000:
        memory->wram[address - 0xC000] = value;
        return;
        //Echo internal RAM 0xE000->0xFE00
    case 0xE000:
        memory->wram[address - 0xE000] = value;
        return;
    case 0xF000:
        switch(address & 0x0F00){
        case 0x000: case 0x100: case 0x200: case 0x300: case 0x400:
        case 0x500: case 0x600: case 0x700: case 0x800: case 0x900:
        case 0xA00: case 0xB00: case 0xC00: case 0xD00:
            memory->wram[address - 0xE000] = value;
            return;
        case 0xE00:
            if(address < 0xFEA0){
                memory->oam[address - 0xFE00] = value;
                update_sprite(address);
                return;
            }else{
                printf("writing to empty ram address %X\n", address);
            }
        case 0xF00:
            if(address == INTERRUPT_ENABLE){
                printf("setting memory interrupts to %X\n",value);
                memory->interrupt_enable = value;
                return;
            }
            if(address < 0xFF80){
                switch(address){
                case 0xFF00://keys
                    display_set_key(value);
                    return;
                case LCD_CONTROL_REGISTER://lcd control register
                    set_lcd_control_register(value);
                    return;
                case LCD_STATUS_REGISTER://lcd status register
                    return;
                case LCD_SCROLL_Y_REGISTER://lcd scroll Y register
                    set_scroll_y(value);
                    return;
                case LCD_SCROLL_X_REGISTER://lcd scroll X register
                    set_scroll_x(value);
                    return;
                case BACKGROUND_PALETTE_MEMORY://palette
                    gpu_set_palette(value,BACKGROUND_PALETTE);
                    return;
                case OBJECT_PALETTE0_MEMORY:
                    gpu_set_palette(value,OBJECT_PALETTE0);
                    return;
                case OBJECT_PALETTE1_MEMORY:
                    gpu_set_palette(value,OBJECT_PALETTE1);
                    return;
                case INTERRUPT_FLAG:
                    printf("setting memory interrupt flag to 0x%X\n",value);
                    memory->interrupt_flags = value;
                    return;
                case DISABLE_BOOT_ROM://disable boot rom
                    if(value == 1)
                        memory->in_bios = 0;
                    else{
                        fprintf(stderr,"cannot disable the boot rom twice\n");
                        exit(1);
                    }
                    return;
                default:
                    //printf("Writing to address 0x%X not handled\n",address);
                    return;
                }
            }
            if(address >= 0xFF80){
                memory->zram[address - 0xFF80] = value;
                return;
            }
        }
    default:
        fprintf(stderr,"Writing to invalid memory address: 0x%X \n",address);
        return;
    }
}

