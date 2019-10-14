#include <stdlib.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "gpu.h"
#include "types.h"
#include "cpu.h"
#include "mem.h"

#define TILE_WIDTH 256
#define TILE_HEIGHT 96

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint32 *pixels; // SDL type unsigned intx
} Window;

Window *window;
Window *tile_window;
Window *disp_mem_window;

Key *key;
Display display;

static void key_up(SDL_Scancode skey){
    switch(skey){
    case SDL_SCANCODE_RETURN://start
        key->rows[0] |= 0x08;
        break;
    case SDL_SCANCODE_LSHIFT://select
        key->rows[0] |= 0x04;
        break;
    case SDL_SCANCODE_X://b
        key->rows[0] |= 0x02;
        break;
    case SDL_SCANCODE_Z://a
        key->rows[0] |= 0x01;
        break;
    case SDL_SCANCODE_DOWN://down
    case SDL_SCANCODE_K:
        key->rows[1] |= 0x08;
        break;
    case SDL_SCANCODE_UP://up
    case SDL_SCANCODE_I:
        key->rows[1] |= 0x04;
        break;
    case SDL_SCANCODE_LEFT://left
    case SDL_SCANCODE_J:
        key->rows[1] |= 0x02;
        break;
    case SDL_SCANCODE_RIGHT://right
    case SDL_SCANCODE_L:
        key->rows[1] |= 0x01;
        break;
    case SDL_SCANCODE_ESCAPE:
        cpu_exit();
        display.exit = 1;
    case SDL_SCANCODE_P:
        cpu_exit();
        break;
    default:
        fprintf(stderr,"Key %s not used\n", SDL_GetScancodeName(skey));
        break;
    }
}

static void key_down(SDL_Scancode skey){//clear the right bit
    switch(skey){
    case SDL_SCANCODE_RETURN://start
        key->rows[0] &= 0x07;
        break;
    case SDL_SCANCODE_LSHIFT://select
        key->rows[0] &= 0x0B;
        break;
    case SDL_SCANCODE_X://b
        key->rows[0] &= 0x0D;
        break;
    case SDL_SCANCODE_Z://a
        key->rows[0] &= 0x0E;
        break;
    case SDL_SCANCODE_DOWN://down
    case SDL_SCANCODE_K:
        key->rows[1] &= 0x07;
        break;
    case SDL_SCANCODE_UP://up
    case SDL_SCANCODE_I:
        key->rows[1] &= 0x0B;
        break;
    case SDL_SCANCODE_LEFT://left
    case SDL_SCANCODE_J:
        key->rows[1] &= 0x0D;
        break;
    case SDL_SCANCODE_RIGHT://right
    case SDL_SCANCODE_L:
        key->rows[1] &= 0x0E;
        break;
    default:
        break;
    }
}

inline u8 display_get_key(){
    switch(key->column){
    case 0x10:
      return key->rows[0];
    case 0x20:
      return key->rows[1];
    default:
        fprintf(stderr,"Key column not recognised %X\n", key->column);
        return 0x0F;
    }
}

inline void display_set_key(u8 value){
  key->column = value & 0x30;
}

void display_get_input(){//gets the current input to the display
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
        case SDL_KEYUP:
            key_up(event.key.keysym.scancode);
            break;
        case SDL_KEYDOWN:
            key_down(event.key.keysym.scancode);
            break;
        case SDL_QUIT:
            cpu_exit();
            display.exit = 1;
            break;
        }
    }
}

void display_redraw(){
    Uint32 *p;
    for(int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            p = &window->pixels[y * WIDTH + x];
            switch(gpu->screen[y][x]){
            case 0:// White
                *p = 0xFFFFFFFF;
                break;
            case 1: // Light gray
                *p = 0xFF808080;
                break;
            case  2: // Dark gray
                *p = 0xFF404040;
                break;
            case 3: // Black
                *p = 0xFF000000;
                break;
	    default: // Red for error pixel
		*p = 0xFFFF0000;
                break;
            }
        }
    }
    SDL_UpdateTexture(window->texture, NULL, window->pixels, WIDTH * sizeof(Uint32));
    SDL_RenderClear(window->renderer);
    SDL_RenderCopy(window->renderer, window->texture, NULL, NULL);
    SDL_RenderPresent(window->renderer);
    display_get_input();//our version of update
}

void display_tile_map() {
    Uint32 *p;

    for(int tile = 0; tile < NUM_TILES; tile++) {
	for(int y = 0; y < 8; y++) {
	    for(int x = 0; x < 8; x++) {
		p = &tile_window->pixels[((tile >> 5) * TILE_WIDTH * 8 ) +
					 (y * TILE_WIDTH) + x + ((tile & 0x1F) * 8)];

		switch(gpu->tiles[tile][y][x]){
		case 0:// White
		    *p = 0xFFFFFFFF;
		    break;
		case 1: // Light gray
		    *p = 0xFF808080;
		    break;
		case  2: // Dark gray
		    *p = 0xFF404040;
		    break;
		case 3: // Black
		    *p = 0xFF000000;
		    break;
		default: // Red for error pixel
		    *p = 0xFFFF0000;
		    break;
		}
	    }
	}
    }
    SDL_UpdateTexture(tile_window->texture, NULL,
		      tile_window->pixels, TILE_WIDTH * sizeof(Uint32));
    SDL_RenderClear(tile_window->renderer);
    SDL_RenderCopy(tile_window->renderer, tile_window->texture, NULL, NULL);
    SDL_RenderPresent(tile_window->renderer);
}

void display_gpu_memory() {
    Uint32 *p;
    u16 memory = 0x8000;
    for(int tile = 0; tile < 384; tile++) {
	for(int y = 0; y < 8; y++) {
	    u8 low = get_mem(memory);
	    u8 high = get_mem(memory + 1);
	    memory += 2;
	    for(int x = 0; x < 8; x++) {
		int sx = 1 << (7 - x);
		p = &disp_mem_window->pixels[((tile >> 5) * TILE_WIDTH * 8 ) +
					    (y * TILE_WIDTH) + x + ((tile & 0x1F) * 8)];
		u8 shade = (low & sx ? 1 : 0) | (high & sx ? 2 : 0);

		switch(shade){
		case 0:// White
		    *p = 0xFFFFFFFF;
		    break;
		case 1: // Light gray
		    *p = 0xFF808080;
		    break;
		case  2: // Dark gray
		    *p = 0xFF404040;
		    break;
		case 3: // Black
		    *p = 0xFF000000;
		    break;
		default: // Red for error pixel
		    *p = 0xFFFF0000;
		    break;
		}
	    }
	}
    }
    SDL_UpdateTexture(disp_mem_window->texture, NULL,
		       disp_mem_window->pixels, TILE_WIDTH * sizeof(Uint32));
    SDL_RenderClear(disp_mem_window->renderer);
    SDL_RenderCopy(disp_mem_window->renderer, disp_mem_window->texture, NULL, NULL);
    SDL_RenderPresent(disp_mem_window->renderer);
}

void display_create_window(Window *w,
		   const char *name,
		   int width,
		   int height) {
    w->pixels = malloc(sizeof(Uint32) * width * height);
    w->window = SDL_CreateWindow(name, SDL_WINDOWPOS_UNDEFINED,
			 SDL_WINDOWPOS_UNDEFINED, width, height,
			 SDL_WINDOW_OPENGL);
    w->renderer = SDL_CreateRenderer(w->window, -1 , 0);
    w->texture = SDL_CreateTexture(w->renderer, SDL_PIXELFORMAT_ARGB8888,
			  SDL_TEXTUREACCESS_STREAMING, width, height);
    if(w->window == NULL || w->renderer == NULL || w->texture == NULL){
        fprintf(stderr,"Unable to set video mode: %s\n", SDL_GetError());
        exit(1);
    }
}


void display_init(){
    window = malloc(sizeof(Window));
    tile_window = malloc(sizeof(Window));
    disp_mem_window = malloc(sizeof(Window));
    //set the keys
    key = malloc(sizeof(Key));
    key->rows[0] = 0x0F;
    key->rows[1] = 0x0F;
    key->column = 0;
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0){
        fprintf(stderr,"Unable to init SDL: %s\n ", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    display_create_window(tile_window, "tile map", TILE_WIDTH, TILE_HEIGHT);
    //display_create_window(disp_mem_window, "display memory map", TILE_WIDTH, TILE_HEIGHT);
    display_create_window(window, "lgb", WIDTH, HEIGHT);
    display.exit = 0;
}
