#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "cpu.h"
#include "mem.h"
#include "defs.h"
#include "bios.h"
#include "gpu.h"
#include "display.h"
#include "timer.h"
#include "sound.h"

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

void mbc_init()
{
    memory->memory_bank_controllers.rom_bank = 0;
    memory->memory_bank_controllers.ram_bank = 0;
    memory->memory_bank_controllers.ram_on = 0;
    memory->memory_bank_controllers.ram_battery = 0;
    memory->memory_bank_controllers.mode = 0;
}

void mem_init(){
    memory = malloc(sizeof(Memory));
    memory->in_bios = 0;
    memory->rom_offset = 0x4000; // Offset for second ROM bank
    memory->ram_offset = 0x0000;
    memory->interrupt_enable = 0;
    memory->debug = 0;
    memory->memory_bank_controller = 0;
    memory->eram_size = 0;
    mbc_init();
}

int load_rom(char* gb_rom_name, char *save_file_name){
    int tmp;
    u8 cartheader[0x150];
    char game_title[0x10];
    int cart_type;
    FILE *gb_rom = fopen(gb_rom_name, "r");
    FILE *save_file = NULL;
    if(!gb_rom)
      return -1;

    for(int i = 0; i < 0x150 && (tmp = getc(gb_rom)) != EOF; i++)
	cartheader[i] = tmp;
    for(int i = 0; i < 0x10; i++)
	game_title[i] = cartheader[0x0134 + i];
    printf("Welcome to %s\n", game_title);
    fseek(gb_rom, 0, SEEK_SET);
    cart_type = cartheader[0x0147];
    memory->rom_banks = cartheader[0x0148];
    memory->ram_banks = cartheader[0x0149];
    printf("cart_type %X rom_banks %d ram banks %d\n",
	   cart_type, memory->rom_banks, memory->ram_banks);

    switch(cart_type){
    case 0:
      memory->memory_bank_controller = 0;
      break;
    case 1:
    case 2:
    case 3:
      memory->memory_bank_controller = 1;
      break;
    case 5:
    case 6:
      memory->memory_bank_controller = 2;
      break;
    case 0x09:
      memory->memory_bank_controllers.ram_battery = 1;
      break;
    case 0x0D:
      memory->memory_bank_controllers.ram_battery = 1;
      break;
    case 0x0F:
      memory->memory_bank_controller = 3;
      break;
    case 0x10:
      memory->memory_bank_controllers.ram_battery = 1;
      break;
    case 0x11: // MBC3
      memory->memory_bank_controller = 3;
      break;
    case 0x12: // MBC3 + RAM
      memory->memory_bank_controller = 3;
      break;
    case 0x13: // MBC3 + RAM + BATTERY
      memory->memory_bank_controller = 3;
      memory->memory_bank_controllers.ram_battery = 1;
      break;
    default:
      printf("Cart type %X MBC controller not implemented\n", cart_type);
    }

    switch(memory->ram_banks){
    case 0:
	memory->eram = NULL;
	memory->eram_size = 0;
	break;
    case 1: // 2KB
	memory->eram = malloc(0x800);
	memory->eram_size = 0x800;
	break;
    case 2: // 8KB
	memory->eram = malloc(0x2000);
	memory->eram_size = 0x2000;
	break;
    case 3: // 32KB
	memory->eram = malloc(0x8000);
	memory->eram_size = 0x8000;
	break;
    default:
      printf("Ram banks %X not supported\n", memory->ram_banks);
      break;
    }

    /* If the game has battery backed RAM it needs to be loaded in */
    if(memory->memory_bank_controllers.ram_battery &&
       access(save_file_name, F_OK) != -1)
      {
	save_file = fopen(save_file_name, "r");
	for(int i = 0; i < memory->eram_size && (tmp = getc(save_file)) != EOF;
	    i++)
	  memory->eram[i] = tmp;
	fclose(save_file);
      }

    memory->rom = malloc(sizeof(u8) * (0x8000 << memory->rom_banks));
    for(int i = 0; i < 0x8000 << memory->rom_banks &&
	  (tmp = getc(gb_rom)) != EOF; i++)
      memory->rom[i] = tmp;
    fclose(gb_rom);

    return 0;
}

void mem_save_ram(char *save_file_name){
  if(memory->memory_bank_controllers.ram_battery)
    {
      FILE *save_file = fopen(save_file_name, "w");
      for(int i = 0; i < memory->eram_size; i++)
	putc(memory->eram[i], save_file);
      fclose(save_file);
    }
}

//get  value at a memory address
u8 get_mem(u16 address){
    switch (address & 0xF000){
        //ROM 32KB
    case 0x0000:
	if(memory->in_bios){
	    if(address < 0x100)
		return bios[address];
	    else {
		if(address == 0x100)
		{
		    memory->in_bios = 0;
		}
		return 0;
	    }
	} else
	    return memory->rom[address];
	break;
    case 0x1000: case 0x2000: case 0x3000:
        return memory->rom[address];

    // Switched memory
    case 0x4000: case 0x5000: case 0x6000: case 0x7000:
      //printf("Accessing memory %X\n", memory->rom_offset + (address & 0x3FFF));
	return memory->rom[memory->rom_offset + (address & 0x3FFF)];

        //Video Ram 2KB
    case 0x8000: case 0x9000:
        return memory->vram[address & 0x1FFF];

        // External switchable ram 2KB
    case 0xA000: case 0xB000:
      if(memory->memory_bank_controllers.ram_on)
	return memory->eram[memory->ram_offset + (address & 0x1FFF)];
      else
	return 0;
        //internal ram 2KB
    case 0xC000: case 0xD000: case 0xE000:
	return memory->wram[address & 0x1FFF];
    case 0xF000:
        switch(address & 0x0F00){
	    // echo internal ram
        case 0x000: case 0x100: case 0x200: case 0x300: case 0x400:
        case 0x500: case 0x600: case 0x700: case 0x800: case 0x900:
        case 0xA00: case 0xB00: case 0xC00: case 0xD00:
	    return memory->wram[address & 0x1FFF];
        case 0xE00:
	    // 160 bytes of oam memory
            if((address & 0xFF) < 0xA0)
                return memory->oam[address & 0xFF];
            else
                printf("OAM read error %X\n", address);
	    return 0;
        case 0xF00:
            if(address < 0xFF80){
                switch(address){
                case SYSTEM_JOYPAD_TYPE_REGISTER://get keys being pressed
                    return display_get_key();
		case SERIAL_TRANSFER_DATA:
		case SIO_CONTROL:
		  return 0; // TODO
                case DIVIDER_REGISTER:
		case TIMER_COUNTER:
		case TIMER_MODULO:
		case TIMER_CONTROL:
		    return timer_read_byte(address);
                case INTERRUPT_FLAG:
                    return memory->interrupt_flags;

		case SOUND_MODE_1_SWEEP_REGISTER:
		  return sound_get_sweep();
		case SOUND_MODE_1_LENGTH_PATTERN_REGISTER:
		  return sound_get_length_wave_pattern(SOUND_CHANNEL_1);
		case SOUND_MODE_1_ENVELOPE_REGISTER:
		  return sound_get_volume(SOUND_CHANNEL_1);
		case SOUND_MODE_1_FREQUENCY_LOW_REGISTER:
		  return sound_get_frequency_low();
		case SOUND_MODE_1_FREQUENCY_HIGH_REGISTER:
		  return sound_get_frequency_high(SOUND_CHANNEL_1);
		case SOUND_MODE_2_SOUND_LENGTH_REGISTER:
		  return sound_get_length_wave_pattern(SOUND_CHANNEL_2);
		case SOUND_MODE_2_ENVELOPE_REGISTER:
		  return sound_get_volume(SOUND_CHANNEL_2);
		case SOUND_MODE_2_FREQUENCY_LOW_REGISTER:
		  return sound_get_frequency_low();
		case SOUND_MODE_2_FREQUENCY_HIGH_REGISTER:
		  return sound_get_frequency_high(SOUND_CHANNEL_2);
		case SOUND_MODE_3_SOUND_ON_OFF_REGISTER:
		  return sound_get_on_off(SOUND_CHANNEL_3);
		case SOUND_MODE_3_SOUND_LENGTH_REGISTER:
		  return sound_get_length_wave_pattern(SOUND_CHANNEL_3);
		case SOUND_MODE_3_OUTPUT_LEVEL_REGISTER:
		  return sound_get_volume(SOUND_CHANNEL_3);
		case SOUND_MODE_3_FREQUENCY_LOW_DATA_REGISTER:
		  return sound_get_frequency_low();
		case SOUND_MODE_3_FREQUENCY_HIGH_DATA_REGISTER:
		  return sound_get_frequency_high(SOUND_CHANNEL_3);
		case SOUND_MODE_4_SOUND_LENGTH_REGISTER:
		  return sound_get_length_wave_pattern(SOUND_CHANNEL_4);
		case SOUND_MODE_4_ENVELOPE_REGISTER:
		  return sound_get_volume(SOUND_CHANNEL_4);
		case SOUND_MODE_4_POLYNOMIAL_COUNTER_REGISTER:
		  return sound_get_frequency_low(SOUND_CHANNEL_4);
		case SOUND_MODE_4_COUNTER_REGISTER:
		  return sound_get_frequency_high(SOUND_CHANNEL_4);
		case SOUND_CHANNEL_CONTROL:
		  return sound_get_channel_control();
		case SOUND_OUTPUT_SELECTION_TERMINAL:
		  return sound_get_output_terminal();
		/*   return sound_get_control()->output_terminal; */
		/* case SOUND_ON_OFF: */
		/*   return sound_get_control()->sound_on_off; */
		case 0xFF30: // wave control registers
		case 0xFF31:
		case 0xFF32:
		case 0xFF33:
		case 0xFF34:
		case 0xFF35:
		case 0xFF36:
		case 0xFF37:
		case 0xFF38:
		case 0xFF39:
		case 0xFF3A:
		case 0xFF3B:
		case 0xFF3C:
		case 0xFF3D:
		case 0xFF3E:
		case 0xFF3F:
		  return sound_get_wave_pattern(address);
                case LCD_CONTROL_REGISTER: // FF40
                    return gpu_get_lcd_control_register();
                case LCD_STATUS_REGISTER:
                    return gpu_get_status_register();
                case LCD_SCROLL_Y_REGISTER:
                    return gpu_get_scroll_y();
                case LCD_SCROLL_X_REGISTER://lcd scroll X register
                    return gpu_get_scroll_x();
                case CURRENT_FRAME_LINE://get the current frame line
                    return gpu_get_line();
		case FRAME_LINE_COMPARE:
		  return gpu_get_line_compare();
		case DMA_TRANSFER:
		  printf("Shouldn't be reading DMA_TRANSFER\n");
		  return 0;
                case BACKGROUND_PALETTE_MEMORY:
                    return gpu_get_palette(BACKGROUND_PALETTE);
                case OBJECT_PALETTE0_MEMORY:
                    return gpu_get_palette(OBJECT_PALETTE0);
                case OBJECT_PALETTE1_MEMORY:
                    return gpu_get_palette(OBJECT_PALETTE1);
		case WINDOW_Y_POS:
		  return gpu_get_window_y();
		case WINDOW_X_POS:
		  return gpu_get_window_x();
                case DISABLE_BOOT_ROM://disable boot rom
		  fprintf(stderr,"Why would you read disabling the boot rom?\n");
		  return 0;

                default:
		  printf("mem.c get_mem should be catching this %X\n",
			 address);
                    return 0;
                }
            }
	    // 128 Bytes of zram
            if(address >= 0xFF80)
	      {
		if(address == INTERRUPT_ENABLE)
		  return memory->interrupt_enable;
		else
		  return memory->zram[address & 0x7F];
	      }
        }
    default:
        fprintf(stderr,"Reading from invalid memory address: %x \n",address);
        return 0;
    }
}

u16 get_mem_16(u16 address){
    return (get_mem(address) <<8) + get_mem(address+1);
}

void set_mem(u16 address, u8 value){
    switch(address & 0xF000){
	// MBC1: External RAM switch
    case 0x0000: case 0x1000:
	switch(memory->memory_bank_controller)
	{
	case 1:
	case 2:
	case 3:
	    memory->memory_bank_controllers.ram_on = (value & 0x0F) == 0x0A;
	    printf("ram on %X\n", value);
	    break;
	}
	return;
	// MBC1: ROM bank
    case 0x2000: case 0x3000:
	switch(memory->memory_bank_controller)
	{
	case 1:	// Set lower 5 bits of ROM bank (skipping #0)
	  value &= 0x1F; // 125 banks only lower 5 bits can be set here
	  if(value == 0) value = 1;
	  memory->memory_bank_controllers.rom_bank &= 0x60;
	  memory->memory_bank_controllers.rom_bank |= value;
	  memory->rom_offset = memory->memory_bank_controllers.rom_bank * 0x4000;
	  break;
	case 2:
	  value &= 0x0F; // 16 banks
	  if(value == 0) value = 1;
	  memory->memory_bank_controllers.rom_bank = value;
	  memory->rom_offset = memory->memory_bank_controllers.rom_bank * 0x4000;
	  break;
	case 3:
	  value &= 0x7F; // 128 banks! Directly mapped
	  if(value == 0) value = 1;
	  memory->memory_bank_controllers.rom_bank = value;
	  memory->rom_offset = memory->memory_bank_controllers.rom_bank * 0x4000;
	  break;
	}
	return;
	// MBC1: RAM bank
    case 0x4000:
    case 0x5000:
	switch(memory->memory_bank_controller)
	{
	case 1:
	case 2:// the same as MBC1 but only 16 rom banks supported
	    if(memory->memory_bank_controllers.mode) {
		// RAM mode set bank
		memory->memory_bank_controllers.ram_bank = value & 0x03;
		memory->ram_offset = memory->memory_bank_controllers.ram_bank * 0x2000;
	    } else {
		// ROM mode: set high bits of bank
		memory->memory_bank_controllers.rom_bank &= 0x1F;
		memory->memory_bank_controllers.rom_bank |= ((value & 0x03) << 5);
		memory->rom_offset = memory->memory_bank_controllers.rom_bank * 0x4000;
	    }
	    break;
	case 3:
	  if(value < 0x04)
	    {
	      memory->memory_bank_controllers.ram_bank = value & 0x03;
	      memory->ram_offset = memory->memory_bank_controllers.ram_bank * 0x2000;
	    }
	  else if(value >= 0x08 && value <= 0x0C) // RTC register select
	      printf("RTC register not supported yet!!\n");
	  else
	    printf("MBC3 ram bank value %X not recognised\n", value);
	  break;
	default:
	  printf("MBC ram bank %X not supported yet\n",
		 memory->memory_bank_controller);
	  break;
	}
	return;
    case 0x6000: case 0x7000:
	switch(memory->memory_bank_controller)
	{
	case 1:
	case 2:
	    memory->memory_bank_controllers.mode = value & 1;
	    break;
	case 3:// latch clock data
	  printf("Latch clock data todo\n");
	  break;
	default:
	  printf("MBC extra %X not supported yet\n",
		 memory->memory_bank_controller);
	}
        return;
        //VRAM 2KB
    case 0x8000: case 0x9000:
        memory->vram[address & 0x1FFF] = value;
        if(address <= TILE_DATA_END)
            gpu_update_tile(address, value);
        return;
        //Swtichable RAM 2KB
    case 0xA000: case 0xB000:
      if(memory->memory_bank_controllers.ram_on)
	memory->eram[memory->ram_offset + (address & 0x1FFF)] = value;
      return;
        //Internal RAM 2KB
    case 0xC000: case 0xD000:
        memory->wram[address & 0x1FFF] = value;
        return;
        //Echo internal RAM 0xE000->0xFE00
    case 0xE000:
        memory->wram[address & 0x1FFF] = value;
        return;
    case 0xF000:
        switch(address & 0x0F00){
        case 0x000: case 0x100: case 0x200: case 0x300: case 0x400:
        case 0x500: case 0x600: case 0x700: case 0x800: case 0x900:
        case 0xA00: case 0xB00: case 0xC00: case 0xD00:
            memory->wram[address & 0x1FFF] = value;
            return;
        case 0xE00:
            if(address < 0xFEA0){
                memory->oam[address & 0xFF] = value;
                gpu_update_sprite(address, value);
            }
	    return;
        case 0xF00:
            if(address < 0xFF80){
                switch(address){
                case SYSTEM_JOYPAD_TYPE_REGISTER://keys
                    display_set_key(value);
                    return;
		case SERIAL_TRANSFER_DATA:
		case SIO_CONTROL:
		  return; // TODO
		case DIVIDER_REGISTER:
		case TIMER_COUNTER:
		case TIMER_MODULO:
		case TIMER_CONTROL:
		    return timer_write_byte(address, value);
                case INTERRUPT_FLAG:
                    memory->interrupt_flags = value;
                    return;

		case SOUND_MODE_1_SWEEP_REGISTER:
		  sound_set_sweep(value);
		  return;
		case SOUND_MODE_1_LENGTH_PATTERN_REGISTER:
		  sound_set_length(SOUND_CHANNEL_1, value);
		  return;
		case SOUND_MODE_1_ENVELOPE_REGISTER:
		  sound_set_volume(SOUND_CHANNEL_1, value);
		  return;
		case SOUND_MODE_1_FREQUENCY_LOW_REGISTER:
		  sound_set_frequency_low(SOUND_CHANNEL_1, value);
		  return;
		case SOUND_MODE_1_FREQUENCY_HIGH_REGISTER:
		  sound_set_frequency_high(SOUND_CHANNEL_1, value);
		  return;
		case SOUND_MODE_2_SOUND_LENGTH_REGISTER:
		  sound_set_length(SOUND_CHANNEL_2, value);
		  return;
		case SOUND_MODE_2_ENVELOPE_REGISTER:
		  sound_set_volume(SOUND_CHANNEL_2, value);
		  return;
		case SOUND_MODE_2_FREQUENCY_LOW_REGISTER:
		  sound_set_frequency_low(SOUND_CHANNEL_2, value);
		  return;
		case SOUND_MODE_2_FREQUENCY_HIGH_REGISTER:
		  sound_set_frequency_high(SOUND_CHANNEL_2, value);
		  return;
		case SOUND_MODE_3_SOUND_ON_OFF_REGISTER:
		  sound_set_on_off(SOUND_CHANNEL_3, value);
		  return;
		case SOUND_MODE_3_SOUND_LENGTH_REGISTER:
		  sound_set_length(SOUND_CHANNEL_3, value);
		  return;
		case SOUND_MODE_3_OUTPUT_LEVEL_REGISTER:
		  sound_set_volume(SOUND_CHANNEL_3, value);
		  return;
		case SOUND_MODE_3_FREQUENCY_LOW_DATA_REGISTER:
		  sound_set_frequency_low(SOUND_CHANNEL_3, value);
		  return;
		case SOUND_MODE_3_FREQUENCY_HIGH_DATA_REGISTER:
		  sound_set_frequency_high(SOUND_CHANNEL_3, value);
		  return;
		case SOUND_MODE_4_SOUND_LENGTH_REGISTER:
		  sound_set_length(SOUND_CHANNEL_4, value);
		  return;
		case SOUND_MODE_4_ENVELOPE_REGISTER:
		  sound_set_volume(SOUND_CHANNEL_4, value);
		  return;
		case SOUND_MODE_4_POLYNOMIAL_COUNTER_REGISTER:
		  sound_set_frequency_low(SOUND_CHANNEL_4, value);
		  return;
		case SOUND_MODE_4_COUNTER_REGISTER:
		  sound_set_frequency_high(SOUND_CHANNEL_4, value);
		  return;
		case SOUND_CHANNEL_CONTROL:
		  sound_set_channel_control(value);
		  return;
		case SOUND_OUTPUT_SELECTION_TERMINAL:
		  sound_set_output_terminal(value);
		  return;
		case SOUND_ON_OFF:
		  sound_set_master_on_off(value);
		  return;
		case 0xFF30:
		case 0xFF31:
		case 0xFF32:
		case 0xFF33:
		case 0xFF34:
		case 0xFF35:
		case 0xFF36:
		case 0xFF37:
		case 0xFF38:
		case 0xFF39:
		case 0xFF3A:
		case 0xFF3B:
		case 0xFF3C:
		case 0xFF3D:
		case 0xFF3E:
		case 0xFF3F:
		  sound_set_wave_pattern(address, value);
		  return;
                case LCD_CONTROL_REGISTER:
                    gpu_set_lcd_control_register(value);
                    return;
                case LCD_STATUS_REGISTER:
		  gpu_set_status_register(value);
		  return;
                case LCD_SCROLL_Y_REGISTER:
                    gpu_set_scroll_y(value);
                    return;
                case LCD_SCROLL_X_REGISTER:
                    gpu_set_scroll_x(value);
                    return;
		case CURRENT_FRAME_LINE:
		  gpu_set_line();
		  return;
		case FRAME_LINE_COMPARE:
		  gpu_set_line_compare(value);
		  return;
		case DMA_TRANSFER:
		  {
		    u16 source_address = value << 8;
		    for(unsigned int i = 0; i < 0xA0; i++)
			set_mem(0xFE00 + i, get_mem(source_address + i));
		  }
		  return;
                case BACKGROUND_PALETTE_MEMORY://palette
                    gpu_set_palette(value, BACKGROUND_PALETTE);
                    return;
                case OBJECT_PALETTE0_MEMORY:
                    gpu_set_palette(value, OBJECT_PALETTE0);
                    return;
                case OBJECT_PALETTE1_MEMORY:
                    gpu_set_palette(value, OBJECT_PALETTE1);
                    return;
		case WINDOW_Y_POS:
		  gpu_set_window_y(value);
		  return;
		case WINDOW_X_POS:
		  gpu_set_window_x(value);
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
		  printf("mem.c should be catching this %X %X\n",
			 address, value);
		  return;
                }
            }
            if(address >= 0xFF80){
	      if(address == INTERRUPT_ENABLE){
                memory->interrupt_enable = value;
                return;
	      }
	      else {
                memory->zram[address & 0x7F] = value;
                return;
	      }
            }
        }
    default:
        fprintf(stderr,"Writing to invalid memory address: 0x%X \n",address);
        return;
    }
}
