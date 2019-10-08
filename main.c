#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "mem.h"
#include "display.h"
#include "gpu.h"
#include "timer.h"

int main(int argc,char **argv){
  char *save_name = "lgb.sav";

    if(argc != 2){
        printf("Usage %s <gameboy rom>\n", argv[0]);
        return 1;
    }

    cpu_init();
    mem_init();
    gpu_init();
    display_init();
    timer_init();

    if(load_rom(argv[1], save_name) == 0){
        while (!display.exit){
            cpu_run();
            display_get_input();
        }
    }else{
        fprintf(stderr,"File not found\n");
        return 1;
    }
    mem_save_ram(save_name);
    return 0;
}
