#include <unistd.h>
#include "gpu.h"
#include "defs.h"
#include "types.h"
#include "cpu.h"
#include "mem.h"
#include "display.h"

GPU *gpu;

void gpu_init(){
    unsigned int x,y;
    gpu = malloc(sizeof(GPU));
    gpu->clock = gpu->mode = 0;
    gpu->line = 0;
    gpu->curscan = 0;
    gpu->scroll_x = 0;
    gpu->scroll_y = 0;

    gpu->lcd_control_register = 0;
    gpu->lcd_display_enable = 0;
    gpu->window_tile_map_display_select = 0x9800;
    gpu->window_display_enable = 0;
    gpu->tile_data_select = 0x8800;
    gpu->background_tile_map_display = 0x9800;
    gpu->background_display_enable = 0;
    gpu->sprite_display_enable = 0;

    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            gpu->screen[y][x] = 0;
            gpu->frame_buffer[y][x] =0;
        }
    }
    memset(gpu->sprites, 0, sizeof(Sprite) * 40);

}

u8 gpu_get_line(){
    return gpu->line & 0xFF;
}
/* Writing to the gpu line register will reset it */
void gpu_set_line(){
  gpu->line = 0;
}
u8 gpu_get_line_compare(){
  return gpu->line_compare & 0xFF;
}
void gpu_set_line_compare(const u8 value){
  gpu->line_compare = value;
}

u8 gpu_get_palette(const PaletteType palette_type){
    switch(palette_type){
    case BACKGROUND_PALETTE:
        return gpu->background_palette;
    case OBJECT_PALETTE0:
        return gpu->object_palette0;
    case OBJECT_PALETTE1:
        return gpu->object_palette1;
    default:
        fprintf(stderr,"gpu_get_palette: PaletteType not recongised\n");
        return 0;
    }
}

void gpu_set_palette(const u8 value, const PaletteType palette_type){
    u8 *palette_colours;
    switch(palette_type){
    case BACKGROUND_PALETTE:
	palette_colours = gpu->background_palette_colours;
	break;
    case OBJECT_PALETTE0:
	palette_colours = gpu->object_palette0_colours;
	break;
    case OBJECT_PALETTE1:
	palette_colours = gpu->object_palette1_colours;
	break;
    default:
        fprintf(stderr, "gpu_set_palette: PaletteType not recongised\n");
        return;
    }
    for(int i = 0; i < 4; i++) {
	switch((value >> (i * 2)) & 0x03) {
	case 0: // White or transparent/ignored on sprites
	    palette_colours[i] = 0;
	    break;
	case 1: // Light gray
	    palette_colours[i] = 1;
	    break;
	case 2: // Dark gray
	    palette_colours[i] = 2;
	    break;
	case 3: // Black
	    palette_colours[i] = 3;
	    break;
	}
    }
}

void gpu_set_scroll_x(const u8 value){
    gpu->scroll_x = value;
}

u8 gpu_get_scroll_x(){
    return gpu->scroll_x;
}

void gpu_set_scroll_y(const u8 value){
    gpu->scroll_y = value;
}

u8 gpu_get_scroll_y(){
  return gpu->scroll_y;
}

void gpu_set_window_x(const u8 value){
  gpu->window_x = value;
}
u8 gpu_get_window_x(){
  return gpu->window_x;
}
void gpu_set_window_y(const u8 value){
  gpu->window_y = value;
}
u8 gpu_get_window_y(){
  return gpu->window_y;
}

void gpu_set_lcd_control_register(const u8 value){
  gpu->lcd_control_register = value;
  gpu->lcd_display_enable = gpu->lcd_control_register & 0x80 ? 1:0;
  gpu->window_tile_map_display_select = gpu->lcd_control_register & 0x40 ?
    0x9C00 : 0x9800;
  gpu->window_display_enable = gpu->lcd_control_register & 0x20 ? 1 : 0;
  gpu->tile_data_select = gpu->lcd_control_register & 0x10 ? 0x8000 : 0x8800;
  gpu->background_tile_map_display = gpu->lcd_control_register & 0x08 ?
    0x9C00 : 0x9800;
  gpu->sprite_size = gpu->lcd_control_register & 0x04 ? 1 : 0;
  gpu->sprite_display_enable = gpu->lcd_control_register & 0x02 ? 1 : 0;
  gpu->background_display_enable = gpu->lcd_control_register & 0x01 ? 1 : 0;
}

u8 gpu_get_lcd_control_register(){
  return gpu->lcd_control_register;
}

void gpu_update_tile(const u16 address, const u8 value){
  /* Takes 2 bytes at a time
   * first byte is LSB of a row
   * second byte is MSB of a row */
  unsigned int addy = address;
  unsigned int tile_num, y, sx, x;
  if(address & 1)
    addy--;
  tile_num = ((addy - 0x8000) >> 4);
  y = (addy >> 1) & 7;
  for(x = 0; x < 8; x++){
    sx = 1 << (7 - x);
    gpu->tiles[tile_num][y][x] = ((get_mem(addy) & sx) ? 1 : 0) |
      ((get_mem(addy + 1) & sx) ? 2 : 0);
  }
  //display_tile_map();
  //display_gpu_memory();
}

void gpu_update_sprite(const u16 address, const u8 value){
  unsigned int sprite_num = (address - 0xFE00) >> 2;
  Sprite *sprite;
  if (sprite_num < 40){
    sprite = &gpu->sprites[sprite_num];
    switch(address & 3){
    case 0:
      sprite->y = value - 16; // can go negative
      break;
    case 1:
      sprite->x = value - 8;
      break;
    case 2:
      sprite->tile = value;
      break;
    case 3:
      sprite->palette = (value & 0x10) ? 1 : 0;
      sprite->xflip = (value & 0x20) ? 1 : 0;
      sprite->yflip = (value & 0x40) ? 1 : 0;
      sprite->prio = (value & 0x80) ? 1 : 0; //0=OBJ Above BG, 1=OBJ Behind BG color 1-3
      break;
    }
  }
}

static void render_scan(){
  if(gpu->background_display_enable){
    unsigned mapoffset = gpu->background_tile_map_display +
      ((((gpu->line + gpu->scroll_y) & 0xFF) >> 3) << 5);
    unsigned lineoffset = (gpu->scroll_x >> 3) & 0x1F;
    unsigned y = (gpu->line + gpu->scroll_y) & 7;
    unsigned x = gpu->scroll_x & 7;

    // Check if the indicies are signed
    if(gpu->tile_data_select == 0x8800) {
      unsigned tile = get_mem(mapoffset + lineoffset);
      if(tile < 128)
	tile += 256;
      u8 *tilerow = gpu->tiles[tile][y];
      for(int i = 0; i < WIDTH; i++){
	//gpu->scanrow[i] = tilerow[x]; needs testing
	gpu->frame_buffer[gpu->line][i] =
	  gpu->background_palette_colours[tilerow[x]];
	x++;
	if(x == 8){
	  lineoffset = (lineoffset + 1) & 0x1F;
	  x = 0;
	  tile = get_mem(mapoffset + lineoffset);
	  if(tile < 128)
	    tile += 256;
	  tilerow = gpu->tiles[tile][y];
	}
      }
    }else {
      u8 *tilerow = gpu->tiles[get_mem(mapoffset + lineoffset)][y];
      for(int i = 0; i < WIDTH; i++)
	{
	  //gpu->scanrow[i] = tilerow[x]; needs testing
	  gpu->frame_buffer[gpu->line][i] =
	    gpu->background_palette_colours[tilerow[x]];
	  x++;
	  if(x == 8) {
	    lineoffset = (lineoffset + 1) & 0x1F;
	    x = 0;
	    tilerow = gpu->tiles[get_mem(mapoffset + lineoffset)][y];
	  }
	}
    }
  }
  if(gpu->window_display_enable && gpu->line >= gpu->window_y){
    unsigned mapoffset = gpu->window_tile_map_display_select +
      ((((gpu->line + gpu->window_y) & 0xFF) >> 3) << 5);
    unsigned lineoffset = (gpu->window_x >> 3) & 0x1F;
    unsigned y = (gpu->line + gpu->window_y) & 7;
    unsigned x = (gpu->window_x - 7) & 7;

    // Indicies are always signed on the window
    unsigned tile = get_mem(mapoffset + lineoffset);
    if(tile < 128)
      tile += 256;
    u8 *tilerow = gpu->tiles[tile][y];
    for(int i = 0; i < WIDTH; i++){
      //palette shared with background
      gpu->frame_buffer[gpu->line][i] =
	gpu->background_palette_colours[tilerow[x]];
      x++;
      if(x == 8){
	lineoffset = (lineoffset + 1) & 0x1F;
	x = 0;
	tile = get_mem(mapoffset + lineoffset);
	if(tile < 128)
	  tile += 256;
	tilerow = gpu->tiles[tile][y];
      }
    }
  }
  if(gpu->sprite_display_enable){
    for(int i = NUM_SPRITES - 1; i >= 0; i--){ // draw in reverse oder
      Sprite *sprite = &gpu->sprites[i];
      if(sprite->y <= gpu->line &&
	 (sprite->y + 8 > gpu->line ||
	  (gpu->sprite_size && (sprite->y + 16) > gpu->line)))
	{
	  u8 *tilerow;
	  if(sprite->yflip) {
	    if(gpu->sprite_size && 15 - gpu->line - sprite->y <  8)
	      tilerow = gpu->tiles[sprite->tile + 1][7 - (gpu->line - sprite->y)];
	    else
	      tilerow = gpu->tiles[sprite->tile][7 - (gpu->line - sprite->y)];
	  } else {
	    if(gpu->sprite_size && gpu->line - sprite->y > 7 )
	      tilerow = gpu->tiles[sprite->tile + 1][gpu->line - sprite->y - 8];
	    else
	      tilerow = gpu->tiles[sprite->tile][gpu->line - sprite->y];
	  }

	  u8 *palette = sprite->palette ? gpu->object_palette1_colours :
	    gpu->object_palette0_colours;

	  for(int x = 0; x < 8; x++){
	    if(sprite->x + x >= 0 && sprite->x + x < WIDTH &&
 		// if the palette index is 0 it's trasparent
		tilerow[sprite->xflip ? (7 - x) : x] &&
		// check background priority BG color 0 is always behind OBJ
	       (!sprite->prio /*|| !gpu->scanrow[sprite->x + x]not sure if this is right*/))
	      {
		gpu->frame_buffer[gpu->line][sprite->x + x] =
		  palette[tilerow[sprite->xflip ? (7 - x) : x]];
	      }
	  }
	}
    }
  }
}

static void swap_buffers(){
  unsigned int x,y;
  for(y=0;y<HEIGHT;y++){
    for(x=0;x<WIDTH;x++){
      gpu->screen[y][x] = gpu->frame_buffer[y][x];
    }
  }
  if(gpu->lcd_display_enable)
    display_redraw();
#define SLEEP
#ifdef SLEEP
  /* Sleep until the frame time is finished */
  struct timespec frame_end_time;
  clock_gettime(CLOCK_MONOTONIC, &frame_end_time);
  long timedelta = FULL_FRAME_TIME_US -
    ((frame_end_time.tv_nsec - gpu->frame_start_time.tv_nsec) / 1000);
  if(timedelta > 0 && timedelta < FULL_FRAME_TIME_US)
    usleep(timedelta);

  /* Set new frame start time */
  clock_gettime(CLOCK_MONOTONIC, &gpu->frame_start_time);
#endif
}

void gpu_step(int op_time){
  gpu->clock += op_time;
  switch(gpu->mode){
  case 0: //Horizontal Blank lasts 4560 clocks including mode 2 and 3
    if(gpu->clock >= HORIZONTAL_BLANK1_TIME){
      if(gpu->line == HEIGHT - 1){//144 vblank start
	gpu->mode = 1;
	memory->interrupt_flags |= 1;
      }else{//Scanline start
	gpu->mode = 2;
      }
      gpu->curscan += 640;
      gpu->clock -= HORIZONTAL_BLANK1_TIME;
    }
    break;
  case 1: //Vertical Blank runs 10 times, 4560 clocks total
    if(gpu->clock >= HORIZONTAL_BLANK2_TIME / 10){
      gpu->clock -= HORIZONTAL_BLANK2_TIME / 10;
      gpu->line++;
      if(gpu->line == (HEIGHT + 10)){
	gpu->line = 0;
	gpu->curscan = 0;
	gpu->mode = 2;
	swap_buffers();
      }
    }
    break;
  case 2: //Scanline accessing OAM
    if(gpu->clock >= SCAN_OAM_TIME){ //goto vram scanning
      gpu->clock -= SCAN_OAM_TIME;
      gpu->mode = 3;
    }
    break;
  case 3: //Scanline accessing VRam
    if(gpu->clock >= SCAN_VRAM_TIME){
      gpu->clock -= SCAN_VRAM_TIME;
      gpu->mode = 0;
      render_scan();
      gpu->line++;
    }
    break;
  }
}
void gpu_set_status_register(const u8 value){
  printf("gpu_set_status_register %X\n", value);
}

u8 gpu_get_status_register(){
  return gpu->mode | (gpu->line == gpu->line_compare ? 0x04 : 0);
}
