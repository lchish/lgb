#include <stdlib.h>
#include <SDL/SDL.h>

#include "display.h"
#include "gpu.h"
#include "types.h"
#include "cpu.h"

static SDL_Surface *surface;
Key *key;
Display display;

static void key_up(SDLKey skey){
    switch(skey){
    case SDLK_RETURN://start
        key->rows[0] |= 0x08;
        break;
    case SDLK_LSHIFT://select
        key->rows[0] |= 0x04;
        break;
    case SDLK_x://b
        key->rows[0] |= 0x02;
        break;
    case SDLK_z://a
        key->rows[0] |= 0x01;
        break;
    case SDLK_DOWN://down
        key->rows[1] |= 0x08;
        break;
    case SDLK_UP://up
        key->rows[1] |= 0x04;
        break;
    case SDLK_LEFT://left
        key->rows[1] |= 0x02;
        break;
    case SDLK_RIGHT://right
        key->rows[1] |= 0x01;
        break;
    case SDLK_ESCAPE:
        cpu_exit();
        display.exit = 1;
    case SDLK_p:
        cpu_exit();
        break;
    case SDLK_s:
        cpu_run_once();
        break;
    default:
        fprintf(stderr,"Key not used\n");
        break;
    }
}

static void key_down(SDLKey skey){//clear the right bit
    switch(skey){
    case SDLK_RETURN://start
        key->rows[0] &= 0x07;
        break;
    case SDLK_RSHIFT://select
        key->rows[0] &= 0x0B;
        break;
    case SDLK_x://b
        key->rows[0] &= 0x0D;
        break;
    case SDLK_z://a
        key->rows[0] &= 0x0E;
        break;
    case SDLK_DOWN://down
        key->rows[1] &= 0x07;
        break;
    case SDLK_UP://up
        key->rows[1] &= 0x0B;
        break;
    case SDLK_LEFT://left
        key->rows[1] &= 0x0D;
        break;
    case SDLK_RIGHT://right
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
            key_up(event.key.keysym.sym);
            break;
        case SDL_KEYDOWN:
            key_down(event.key.keysym.sym);
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
    Uint8 *p;
    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            p=(Uint8 *)surface->pixels+y * surface->pitch+
                x * surface->format->BytesPerPixel;
            switch(gpu->screen[y][x]){
            case 0:
                *p = 0xFF;//white
                break;
            case 1:
                *p = 0xFF/2;
                break;
            case  2:
                *p = 0xFF/4;
                break;
            case 3:
                *p = 0x0;//black
                break;
            }
        }
    }
    SDL_Flip(surface);
    display_get_input();//our version of update
}

void display_clear(){
    int x,y;
    Uint8 *p;
    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            p=(Uint8 *)surface->pixels+y * surface->pitch+x * surface->format->BytesPerPixel; 
            *p =0;
        }
    }
    SDL_Flip(surface);
}

void display_init(){
    //set the keys
    int i;
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
    surface = SDL_SetVideoMode(WIDTH,HEIGHT,8,SDL_HWSURFACE);
    if(surface == NULL){
        fprintf(stderr,"Unable to set video mode: %s",SDL_GetError());
        exit(1);
    }
    display.exit = 0;
}
