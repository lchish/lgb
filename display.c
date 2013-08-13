#include <stdlib.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "gpu.h"
#include "types.h"
#include "cpu.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

Key *key;
Display display;
Uint32 *pixels;

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
        key->rows[1] |= 0x08;
        break;
    case SDL_SCANCODE_UP://up
        key->rows[1] |= 0x04;
        break;
    case SDL_SCANCODE_LEFT://left
        key->rows[1] |= 0x02;
        break;
    case SDL_SCANCODE_RIGHT://right
        key->rows[1] |= 0x01;
        break;
    case SDL_SCANCODE_ESCAPE:
        cpu_exit();
        display.exit = 1;
    case SDL_SCANCODE_P:
        cpu_exit();
        break;
    case SDL_SCANCODE_S:
        cpu_run_once();
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
        key->rows[1] &= 0x07;
        break;
    case SDL_SCANCODE_UP://up
        key->rows[1] &= 0x0B;
        break;
    case SDL_SCANCODE_LEFT://left
        key->rows[1] &= 0x0D;
        break;
    case SDL_SCANCODE_RIGHT://right
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
        fprintf(stderr,"Key not recognised %d\n",key->column);
        return 0;
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
    int x,y;
    Uint32 *p;
    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            p = &pixels[y * WIDTH + x];

            switch(gpu->screen[y][x]){
            case 0:
                *p = 0xFFFFFFFF;//white
                break;
            case 1:
                *p = 0xFF808080;
                break;
            case  2:
                *p = 0xFF404040;
                break;
            case 3:
                *p = 0xFF000000;//black
                break;
            }
        }
    }
    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer,texture,NULL,NULL);
    SDL_RenderPresent(renderer);
    display_get_input();//our version of update
}

void display_init(){
    int i;
    pixels = malloc(sizeof(Uint32) * WIDTH * HEIGHT);
    //set the keys
    key = malloc(sizeof(Key));
    for(i=0;i<2;i++){
        key->rows[i]=0x0F;
    }
    key->column = 0;
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        fprintf(stderr,"Unable to init SDL: %s",SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    window = SDL_CreateWindow("lgb", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_OPENGL);
    renderer = SDL_CreateRenderer(window, -1 , 0);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if(window == NULL || renderer == NULL || texture == NULL){
        fprintf(stderr,"Unable to set video mode: %s",SDL_GetError());
        exit(1);
    }
    display.exit = 0;
}
