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
    gpu->clock = gpu->mode = gpu->line = 0;
    gpu->scroll_x = gpu->scroll_y=0;

    gpu->lcd_control_register = 0;
    gpu->lcd_display_enable = 0;
    gpu->window_tile_map_display_select = 0x9800;
    gpu->window_display_enable = 0;
    gpu->tile_data_select = 0x8800;
    gpu->background_tile_map_display = 0x9800;

    gpu->window_display_enable = 0;

    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            gpu->screen[y][x] = 0;
            gpu->frame_buffer[y][x] =0;
        }
    }
    memset(gpu->sprites,0,sizeof(Sprite)*40);
  
}

u8 gpu_get_line(){
    return gpu->line & 0xFF;
}

u8 gpu_get_palette(const PaletteType palette_type){
    switch(palette_type){
    case BACKGROUND_PALETTE:
        return gpu->background_palette;
    case OBJECT_PALETTE0:
        return gpu->object_palette0;
    case OBJECT_PALETTE1:
        return gpu->object_palette0;
    default:
        fprintf(stderr,"gpu_get_palette: PaletteType not recongised\n");
        return 0;
    }
}

void gpu_set_palette(const u8 value, const PaletteType palette_type){
    u8 *palette;
    u8 *palette_colours;
    switch(palette_type){
    case BACKGROUND_PALETTE:
        palette = &gpu->background_palette;
        palette_colours = gpu->background_palette_colours;
        break;
    case OBJECT_PALETTE0:
        palette = &gpu->object_palette0;
        palette_colours = gpu->object_palette0_colours;
        break;
    case OBJECT_PALETTE1:
        palette = &gpu->object_palette0;
        palette_colours = gpu->object_palette0_colours;
        break;
    default:
        fprintf(stderr,"gpu_set_palette: PaletteType not recongised\n");
        return;
    }
    *palette = value;
    palette_colours[0] = (*palette & 0x01 ? 1:0)  + 
        (*palette & 0x02 ? 2:0);
    palette_colours[1] = (*palette & 0x04 ? 1:0)  + 
        (*palette & 0x08 ? 2:0);
    palette_colours[2] = (*palette & 0x10 ? 1:0)  + 
        (*palette & 0x20 ? 2:0);
    palette_colours[3] = (*palette & 0x40 ? 1:0)  + 
        (*palette & 0x80 ? 2:0);
}

void set_scroll_x(const u8 value){
    gpu->scroll_x = value;
}

u8 get_scroll_x(){
    return gpu->scroll_x;
}

void set_scroll_y(const u8 value){
    gpu->scroll_y = value;
}

u8 get_scroll_y(){
    return gpu->scroll_y;
}

void set_lcd_control_register(const u8 value){
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
    gpu->background_display = gpu->lcd_control_register & 0x01 ? 1 : 0;
}

u8 get_lcd_control_register(){
    return gpu->lcd_control_register;
}

void update_tile(const u16 address){
    unsigned int addy = address,tile_num,y,sx,x;
    if(address&1)addy--;
    tile_num = ((addy - 0x8000)>>4);
    y=(addy>>1)&7;
    for(x=0;x<8;x++){
        sx=(1<<(7-x));
        gpu->tiles[tile_num][y][x] = (get_mem(addy) & sx) ?1:0 | 
            (get_mem(addy+1) & sx)?2:0;
    }
}
void update_sprite(const u16 address){
    unsigned int sprite_num = (address - 0xFE00) >> 2;
    Sprite *sprite;
    u8 val = get_mem(address);
    
    if (sprite_num < 40){
        sprite = gpu->sprites[sprite_num];
        switch(address & 3){
        case 0:
            sprite->y = val-16;
            break;
        case 1:
            sprite->x = val-8;
            break;
        case 2:
            sprite->tile = val;
            break;
        case 3:
            sprite->palette = (val & 0x10) ? 1 : 0;
            sprite->xflip = (val & 0x20) ? 1 : 0;
            sprite->yflip = (val & 0x40) ? 1 : 0;
            sprite->prio = (val & 0x80) ? 1 : 0;
            break;
        }
    }
}
static void render_scan(){
    unsigned int i,mapoffset,lineoffset,x,y,tile;
    Sprite *sprite;
    if(gpu->background_display){
        mapoffset=((gpu->line+gpu->scroll_y)&255)>>3;
        lineoffset = gpu->scroll_x >> 3;
        y = (gpu->line+gpu->scroll_y) & 7;
        x = gpu->scroll_x&7;
        tile = get_mem(gpu->background_tile_map_display + (mapoffset *32) + lineoffset);
        if(gpu->tile_data_select == 0x8800 && tile < 127 )//unsigned
            tile+=255;
        for(i=0;i<WIDTH;i++){
            gpu->frame_buffer[gpu->line][i] = gpu->tiles[tile][y][x];
            x++;
            if(x==8){
                x=0;
                lineoffset = (lineoffset+1)&31;
                tile = get_mem(gpu->background_tile_map_display + mapoffset * 32 + lineoffset);
                if(gpu->tile_data_select == 0x8800 && tile < 127 )
                    tile += 255;
            }
        }
    }

    //need to add palette support sometime
    if(gpu->sprite_display_enable){
        for(i = 0; i < NUM_SPRITES; i++){
            sprite = gpu->sprites[i];
            if( sprite->y <= gpu->line && (sprite->y + 8) > gpu->line){
                for( x = 0; x < 8; x++){
                    if ((sprite->x + x) >= 0 && (sprite->x + x ) < WIDTH &&
                        gpu->tiles[sprite->tile][gpu->line - sprite->y][x]){
                        gpu->frame_buffer[gpu->line][sprite->x+x] = gpu->object_palette0_colours[2];
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
}

void gpu_step(int op_time){
    gpu->clock += op_time;
    switch(gpu->mode){
    case 0://Horizontal Blank
        if(gpu->clock >= HORIZONTAL_BLANK1_TIME){
            gpu->clock = 0;
            gpu->line++;
            if(gpu->line == (HEIGHT-1)){//vblank
                gpu->mode = 1;
                //push the stored image onto the screen
            }else{//Scanline start
                gpu->mode=2;
            }
        }
        break;
    case 1: //Vertical Blank runs 10 times
        if(gpu->clock >=HORIZONTAL_BLANK2_TIME){
            gpu->clock = 0;
            gpu->line++;
            if(gpu->line == (HEIGHT+9)){
                gpu->mode = 2;
                gpu->line = 0;
                swap_buffers();
            }
        }
        break;
    case 2: //Scanline accessing OAM
        if(gpu->clock >= SCAN_OAM_TIME){ //goto vram scanning
            gpu->clock = 0;
            gpu->mode = 3;
        }
        break;
    case 3: //Scanline accessing VRam
        if(gpu->clock >= SCAN_VRAM_TIME){
            gpu->clock = 0;
            gpu->mode = 0;
            render_scan();
        }
        break;
    }
}

u8 gpu_get_status_register(){
    return gpu->mode;
}

static void print_screen(){
    unsigned int x,y;
    for(y=0;y<HEIGHT;y++){
        for(x=0;x<WIDTH;x++){
            printf("%s",gpu->screen[y][x]==0?" ":"a");
        }
        printf("\n");
    }
}

static void print_tiles(){
    unsigned int i,j,k;
    for(i=0;i<27;i++){
        printf("Tile : %d\n",i);
        for(j=0;j<8;j++){
            printf("row %d:",j);
            for(k=0;k<8;k++){
                printf("%s",gpu->tiles[i][j][k]==1? "a" :" ");
            }
            printf("\n");
        }
    }
}

static  void print_tile_ram(){
    unsigned int i,j;
    for(i=0;i<26;i++){
        printf("Tile %X:\n",i);
        for(j=0;j<16;j++){
            printf("memory %d : %X\n",j,get_mem(0x8000 + i*16 + j));
        }
    }
}

static  void print_background_data_ram(){
    unsigned int i;
    for(i=0;i<=0x2f;i++){
        printf("memory %X : %d\n",0x9900+i,get_mem(0x9900 + i));
    }
}
  
void gpu_test(){
    print_screen();
    print_tiles();
    print_tile_ram();
    print_background_data_ram();
}
