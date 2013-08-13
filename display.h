#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include "types.h"
#include "defs.h"

typedef struct {
    u8 rows[2];
    u8 column;
}Key;

typedef struct{
    int exit;
}Display;

u8 display_get_key();
void display_set_key(u8 value);
void display_init();
void display_redraw();
void display_get_input();

extern Display display;
#endif
