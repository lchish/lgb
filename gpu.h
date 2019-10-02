#ifndef GPU_H
#define GPU_H

#include <time.h>
#include "defs.h"
#include "types.h"

#define WIDTH 160
#define HEIGHT 144
#define FULL_FRAME 70224//number of clock cycles a frame, approx 60 fps

#define HORIZONTAL_BLANK1_TIME 204
#define HORIZONTAL_BLANK2_TIME 4560
#define SCAN_VRAM_TIME 172
#define SCAN_OAM_TIME 80

#define NUM_TILES 384
#define NUM_SPRITES 40

#define VIDEO_RAM_START 0x8000
#define VIDEO_RAM_END 0x9FFF
#define TILE_DATA_END 0x97FF

#define FULL_FRAME_TIME_US 16667 /* microseconds */

typedef enum {
    BACKGROUND_PALETTE,
    OBJECT_PALETTE0,
    OBJECT_PALETTE1
}PaletteType;

typedef struct{
    int tile;
    u8 x;
    u8 y;
    int palette;
    int xflip;
    int yflip;
    int prio;
} Sprite;

typedef struct{
    u8 tiles[NUM_TILES][8][8];
    u8 screen[HEIGHT] [WIDTH];
    Sprite sprites[NUM_SPRITES];

    u8 frame_buffer[HEIGHT][WIDTH];
    int clock;
//scroll registers
    u8 scroll_x;
    u8 scroll_y;
    u8 window_x;
    u8 window_y;

    int line;
    int line_compare;
    int mode;
    int curscan;

    /* Internal to the emulator */
    struct timespec frame_start_time;

/*lcd control register stuff */
    u8 lcd_control_register;

    int lcd_display_enable;
    u16 window_tile_map_display_select;// 0 = 9800-9BFF, 1 = 9C00-9FFF
    int window_display_enable;
    u16 tile_data_select;// 0 = 8800-97FF, 1 = 8000-8FFF
    u16 background_tile_map_display;// 0 = 9800-9BFF, 1 = 9C00-9FFF
    int sprite_size;
    int sprite_display_enable;
    int background_display_enable;

    u8 background_palette;
    u8 object_palette0;
    u8 object_palette1;
    u8 background_palette_colours[4];
    u8 object_palette0_colours[4];
    u8 object_palette1_colours[4];

    u8 scanrow[1000];

} GPU;

extern GPU *gpu;
//extern int gpu_clock;
extern void gpu_init();
extern u8 gpu_get_line();
extern void gpu_set_line();
extern u8 gpu_get_line_compare();
extern void gpu_set_line_compare(u8 value);
extern void gpu_step(const int op_time);
extern u8 gpu_get_status_register();
extern void gpu_set_status_register(const u8 value);
extern u8 gpu_get_palette(const PaletteType palette_type);
extern void gpu_set_palette(const u8 value, const PaletteType palette_type);
extern void gpu_set_scroll_y(const u8 value);
extern u8 gpu_get_scroll_y();
extern void gpu_set_scroll_x(const u8 value);
extern u8 gpu_get_scroll_x();
extern void gpu_set_window_y(const u8 value);
extern u8 gpu_get_window_y();
extern void gpu_set_window_x(const u8 value);
extern u8 gpu_get_window_x();
extern void gpu_set_lcd_control_register(const u8 value);
extern u8 gpu_get_lcd_control_register();
extern void gpu_update_tile(const u16 address, const u8 value);
extern void gpu_update_sprite(const u16 address, const u8 value);
#endif
