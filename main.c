#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "mem.h"
#include "display.h"
#include "gpu.h"
#include "timer.h"

int main(int argc,char **argv){
    FILE *gb_file;

    if(argc != 2){
        printf("You gotta play with a game fool\n");
        return 1;
    }

    cpu_init();
    mem_init();
    gpu_init();
    display_init();
    timer_init();

    gb_file = fopen(argv[1],"r");

    if(gb_file){
        load_rom(gb_file);
        while (!display.exit){
            cpu_run();
            display_get_input();
        }
    }else{
        fprintf(stderr,"File not found\n");
        return 1;
    }

    return 0;
}
