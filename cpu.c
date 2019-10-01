#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include "cpu.h"
#include "cpu_timings.h"
#include "types.h"
#include "defs.h"
#include "mem.h"
#include "gpu.h"
#include "timer.h"

Cpu *cpu;

static unsigned int load_json(const json_object *med_obj,
			      const unsigned int opcode, const int pos,
			      const char *prefix);
json_object *med_obj;


static inline u8 read(u16 addr){
    u8 ret = get_mem(addr);
    cpu->cycle_counter += 4;
    return ret;
}

static inline u8 pc_read(){
    u8 ret = read(cpu->PC);
    cpu->PC = (cpu->PC + 1) & 0xFFFF;
    return ret;
}

static inline void write(u16 addr, u8 value){
    set_mem(addr, value);
    cpu->cycle_counter += 4;
}

static inline void pc_change(u16 value){
    cpu->PC = value;
    cpu->cycle_counter += 4;
}

static inline void push_stack(u8 high, u8 low){
    cpu->SP = (cpu->SP - 1) & 0xFFFF;
    write(cpu->SP, high);
    cpu->SP = (cpu->SP - 1) & 0xFFFF;
    write(cpu->SP, low);
}

static inline void call_routine(u16 address){
    push_stack((cpu->PC >> 8) & 0xFF, cpu->PC & 0xFF);
    pc_change(address);
}
// relative jump by signed immediate
static inline void rjsi(){
    unsigned v = pc_read();
    v = (v ^ 0x80) - 0x80;
    pc_change((cpu->PC + v) & 0xFFFF);

}
//convert two u8s to a single u16
static inline u16 u8_to_u16(const u8 high, const u8 low)
{
    return (u16) (((high << 8) & 0xFF00) + low);
}
//return
static inline void ret(){
    pc_change(u8_to_u16(read(cpu->SP + 1), (read(cpu->SP))));
    cpu->SP = (cpu->SP + 2) & 0xFFFF;
    memory->debug = 0;
}
static inline void absolute_jump(){
    pc_change(u8_to_u16(read(cpu->PC + 1),read(cpu->PC)));
}
//set/unset flags
static inline void set_zero()
{
    cpu->F |= 0x80;
}
static inline void unset_zero()
{
    cpu->F &= ~0x80;
}
static inline void set_subtract()
{
    cpu->F |= 0x40;
}
static inline void unset_subtract()
{
    cpu->F &= ~0x40;
}
static inline void set_halfcarry()
{
    cpu->F |= 0x20;
}
static inline void unset_halfcarry()
{
    cpu->F &= ~0x20;
}
static inline void set_carry()
{
    cpu->F |= 0x10;
}
static inline void unset_carry()
{
    cpu->F &= ~0x10;
}
static inline void reset_flags()
{
    cpu->F = 0;
}
//arithmetic operations
static inline u8 inc_8(const u8 to_inc)
{
    unset_subtract();
    unset_halfcarry();
    unset_zero();
    if((to_inc & 0xF) == 0xF)
        set_halfcarry();
    if(to_inc == 0xFF) {
        set_zero();
        return 0;
    }
    return to_inc + 1;
}
static inline u8 dec_8(const u8 to_dec)
{
    set_subtract();
    unset_halfcarry();
    unset_zero();
    if((to_dec & 0xF) == 0)
	set_halfcarry();
    if(to_dec == 1)
        set_zero();
    return to_dec == 0 ? 0xFF : to_dec - 1;
}
static inline u8 add_8(const u8 a,const u8 b)
{
    unsigned int ret = a + b;
    reset_flags();
    if(!(ret & 0xFF)) set_zero();
    if(ret > 0xFF) set_carry();
    if((a & 0xF) + (b & 0xF) > 0xF) set_halfcarry();
    return ret & 0xFF;
}
static inline u8 add_8c(const u8 a,const u8 b) //also add carry flag
{
    unsigned int carry = (cpu->F >> 4) & 1;
    unsigned int ret = a + b + carry;
    reset_flags();
    if(!(ret & 0xFF)) set_zero();
    if(ret > 0xFF) set_carry();
    if((a & 0xF) + (b & 0xF) + carry > 0xF) set_halfcarry();
    return ret & 0xFF;
}
static inline u8 sub_8(const u8 a,const u8 b)
{
    u8 ret = (a - b) & 0xFF;
    reset_flags();
    set_subtract();
    if(!(ret & 0xFF)) set_zero();
    if(a - b < 0) set_carry();
    if((a & 0xF) - (b & 0xF) < 0) set_halfcarry();
    return ret;
}

static inline u8 sub_8c(const u8 a,const u8 b)
{
    int carry = (cpu->F >> 4) & 1;
    int result = a - b - carry;
    reset_flags();
    set_subtract();
    if(!(result & 0xFF))
        set_zero();
    if(result < 0)
        set_carry();
    if((a & 0xF) - (b & 0xF) - carry < 0)
        set_halfcarry();
    return (result & 0xFF);
}
static inline u16 inc_16(const u16 to_inc)   //sets no flags
{
    return (to_inc + 1) & 0xFFFF;
}
static inline u16 dec_16(const u16 to_dec)
{
    return (to_dec - 1) & 0xFFFF;
}
static inline u16 add_16(const u16 a,const u16 b)
{
    unset_subtract();
    unset_halfcarry();
    unset_carry();
    unsigned int ret = a + b;
    if(ret > 0xFFFF) set_carry();
    if((a & 0x07FF) + (b & 0x07FF) > 0x07FF) set_halfcarry();
    return ret & 0xFFFF;
}
//logic
static inline u8 and_8(const u8 a,const u8 b)
{
    u8 ret = a & b;
    reset_flags();
    set_halfcarry();
    if(!ret) set_zero();
    return ret;
}
static inline u8 or_8(const u8 a,const u8 b)
{
    u8 ret = a | b;
    reset_flags();
    if(!ret) set_zero();
    return ret;
}
static inline u8 xor_8(const u8 a,const u8 b)
{
    u8 ret = a ^ b;
    reset_flags();
    if(!ret) set_zero();
    return ret;
}

static inline u8 rot_left_carry_8(const u8 a)
{
    reset_flags();
    if(a & 0x80) set_carry();
    return ((a << 1) | (a >> 7)) & 0xFF;
}
static inline u8 rot_left_8(const u8 a)
{
    u8 ret;
    ret = ((a << 1) | ((cpu->F >> 4) & 0x01)) & 0xFF;
    reset_flags();
    if (a & 0x80) set_carry();
    return ret;
}
static inline u8 rot_right_carry_8(const u8 a)
{
    reset_flags();
    if (a & 0x01) set_carry();
    return ((a >> 1) | (a << 7)) & 0xFF;
}
static inline u8 rot_right_8(const u8 a)
{
    u8 ret;
    ret = ((a >> 1) | (cpu->F & 0x10 ? 0x80 : 0x00)) & 0xFF;
    reset_flags();
    if (a & 0x01) set_carry();
    return ret;
}
//compare
static inline void comp_8(const u8 a,const u8 b)
{
    int ret = a - b;
    reset_flags();
    set_subtract();
    if(!ret) set_zero();
    if(ret < 0) set_carry();
    if((a & 0xF) - (b & 0xF ) < 0) set_halfcarry();
}
static inline void call_nn()
{
    unsigned npc = (cpu->PC + 2) & 0xFFFF;
    pc_change(u8_to_u16(read(cpu->PC + 1), read(cpu->PC)));
    push_stack((npc >> 8) & 0xFF, npc & 0xFF);
}


void cb_opcodes(const u8 opcode)
{
    u16 tmp_address;

    switch(opcode) {
    case 0x00://Rotate cpu->B left with carry
        cpu->B = rot_left_carry_8(cpu->B);
        if(!cpu->B) set_zero();
        break;
    case 0x01://Rotate cpu->C left with carry
        cpu->C = rot_left_carry_8(cpu->C);
        if(!cpu->C) set_zero();
        break;
    case 0x02://Rotate cpu->D left with carry
        cpu->D = rot_left_carry_8(cpu->D);
        if(!cpu->D) set_zero();
        break;
    case 0x03://Rotate E left with carry
        cpu->E = rot_left_carry_8(cpu->E);
        if(!cpu->E) set_zero();
        break;
    case 0x04://Rotate cpu->H left with carry
        cpu->H = rot_left_carry_8(cpu->H);
        if(!cpu->H) set_zero();
        break;
    case 0x05://Rotate cpu->L left with carry
        cpu->L = rot_left_carry_8(cpu->L);
        if(!cpu->L) set_zero();
        break;
    case 0x06://Rotate value pointed by cpu->Hcpu->L left with carry
        tmp_address = u8_to_u16(cpu->H, cpu->L);
        write(tmp_address,rot_left_carry_8(read(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x07://Rotate A left with carry
        cpu->A = rot_left_carry_8(cpu->A);
        if(!cpu->A) set_zero();
        break;
    case 0x08://Rotate cpu->B right with carry
        cpu->B = rot_right_carry_8(cpu->B);
        if(!cpu->B) set_zero();
        break;
    case 0x09://Rotate cpu->C right with carry
        cpu->C = rot_right_carry_8(cpu->C);
        if(!cpu->C) set_zero();
        break;
    case 0x0A://Rotate cpu->D right with carry
        cpu->D = rot_right_carry_8(cpu->D);
        if(!cpu->D) set_zero();
        break;
    case 0x0B://Rotate E right with carry
        cpu->E = rot_right_carry_8(cpu->E);
        if(!cpu->E) set_zero();
        break;
    case 0x0C://Rotate cpu->H right with carry
        cpu->H = rot_right_carry_8(cpu->H);
        if(!cpu->H) set_zero();
        break;
    case 0x0D://Rotate cpu->L right with carry
        cpu->L = rot_right_carry_8(cpu->L);
        if(!cpu->L) set_zero();
        break;
    case 0x0E://Rotate value pointed by cpu->Hcpu->L right with carry
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        write(tmp_address,rot_right_carry_8(read(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x0F://Rotate cpu->A right with carry
        cpu->A = rot_right_carry_8(cpu->A);
        if(!cpu->A) set_zero();
        break;

    case 0x10://Rotate cpu->B left
        cpu->B = rot_left_8(cpu->B);
        if(!cpu->B) set_zero();
        break;
    case 0x11://Rotate cpu->C left
        cpu->C = rot_left_8(cpu->C);
        if(!cpu->C) set_zero();
        break;
    case 0x12://Rotate cpu->D left
        cpu->D = rot_left_8(cpu->D);
        if(!cpu->D) set_zero();
        break;
    case 0x13://Rotate E left
        cpu->E = rot_left_8(cpu->E);
        if(!cpu->E) set_zero();
        break;
    case 0x14://Rotate cpu->H left
        cpu->H = rot_left_8(cpu->H);
        if(!cpu->H) set_zero();
        break;
    case 0x15://Rotate cpu->L left
        cpu->L = rot_left_8(cpu->L);
        if(!cpu->L) set_zero();
        break;
    case 0x16://Rotate value pointed by cpu->Hcpu->L left
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        write(tmp_address,rot_left_8(read(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x17://Rotate A left
        cpu->A = rot_left_8(cpu->A);
        if(!cpu->A) set_zero();
        break;
    case 0x18://Rotate B right
        cpu->B = rot_right_8(cpu->B);
        if(!cpu->B) set_zero();
        break;
    case 0x19://Rotate C right
        cpu->C = rot_right_8(cpu->C);
        if(!cpu->C) set_zero();
        break;
    case 0x1A://Rotate cpu->D right
        cpu->D = rot_right_8(cpu->D);
        if(!cpu->D) set_zero();
        break;
    case 0x1B://Rotate E right
        cpu->E = rot_right_8(cpu->E);
        if(!cpu->E) set_zero();
        break;
    case 0x1C://Rotate cpu->H right
        cpu->H = rot_right_8(cpu->H);
        if(!cpu->H) set_zero();
        break;
    case 0x1D://Rotate cpu->L right
        cpu->L = rot_right_8(cpu->L);
        if(!cpu->L) set_zero();
        break;
    case 0x1E://Rotate value pointed by cpu->Hcpu->L right
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        write(tmp_address,rot_right_8(read(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x1F://Rotate cpu->A right
        cpu->A = rot_right_8(cpu->A);
        if(!cpu->A) set_zero();
        break;

    case 0x20://Shift cpu->B left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->B & 0x80)
            set_carry();
        cpu->B = (cpu->B << 1) & 0xFF;
        if(!cpu->B)
            set_zero();
        break;
    case 0x21://Shift cpu->C left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->C & 0x80)
            set_carry();
        cpu->C = (cpu->C << 1) & 0xFF;
        if(!cpu->C)
            set_zero();
        break;
    case 0x22://Shift cpu->D left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->D & 0x80)
            set_carry();
        cpu->D = (cpu->D << 1) & 0xFF;
        if(!cpu->D)
            set_zero();
        break;
    case 0x23://Shift E left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->E & 0x80)
            set_carry();
        cpu->E = (cpu->E << 1) & 0xFF;
        if(!cpu->E)
            set_zero();
        break;
    case 0x24://Shift cpu->H left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->H & 0x80)
            set_carry();
        cpu->H = (cpu->H << 1) & 0xFF;
        if(!cpu->H)
            set_zero();
        break;
    case 0x25://Shift cpu->L left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->L & 0x80)
            set_carry();
        cpu->L = (cpu->L << 1) & 0xFF;
        if(!cpu->L)
            set_zero();
        break;
    case 0x26://Shift value pointed to by cpu->Hcpu->L left into carry cpu->LScpu->B set to 0
        reset_flags();
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        if(get_mem(tmp_address) & 0x80)
            set_carry();
        write(tmp_address, (read(tmp_address) << 1) & 0xFF);
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x27://Shift n left into carry cpu->LScpu->B set to 0
        reset_flags();
        if(cpu->A & 0x80)
            set_carry();
        cpu->A = (cpu->A << 1) & 0xFF;
        if(!cpu->A)
            set_zero();
        break;
    case 0x28://Shift cpu->B right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->B & 1)
            set_carry();
        cpu->B = (cpu->B >> 1) | (cpu->B & 0x80);
        if(!cpu->B)
            set_zero();
        break;
    case 0x29://Shift cpu->C right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->C & 1)
            set_carry();
        cpu->C = (cpu->C >> 1) | (cpu->C & 0x80);
        if(!cpu->C)
            set_zero();
        break;
    case 0x2A://Shift cpu->D right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->D & 1)
            set_carry();
        cpu->D = (cpu->D >> 1) | (cpu->D & 0x80);
        if(!cpu->D)
            set_zero();
        break;
    case 0x2B://Shift E right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->E & 1)
            set_carry();
        cpu->E = (cpu->E >> 1) | (cpu->E & 0x80);
        if(!cpu->E)
            set_zero();
        break;
    case 0x2C://Shift cpu->H right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->H & 1)
            set_carry();
        cpu->H = (cpu->H >> 1) | (cpu->H & 0x80);
        if(!cpu->H)
            set_zero();
        break;
    case 0x2D://Shift cpu->L right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->L & 1)
            set_carry();
        cpu->L = (cpu->L >> 1) | (cpu->L & 0x80);
        if(!cpu->L)
            set_zero();
        break;
    case 0x2E://Shift memory at cpu->Hcpu->L right into carry. MScpu->B doesn't change
        reset_flags();
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        if(get_mem(tmp_address) & 1)
            set_carry();
        set_mem(tmp_address, (read(tmp_address) >> 1) |
		(read(tmp_address) & 0x80));
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x2F://Shift cpu->A right into carry. MScpu->B doesn't change
        reset_flags();
        if(cpu->A & 1)
            set_carry();
        cpu->A = (cpu->A >> 1) | (cpu->A & 0x80);
        if(!cpu->A)
            set_zero();
        break;

    case 0x30://swap nibbles in cpu->B
        reset_flags();
        cpu->B = (((cpu->B & 0xF0) >> 4) & 0x0F) | (((cpu->B & 0x0F) << 4 ) & 0xF0);
        if(!cpu->B)set_zero();
        break;
    case 0x31://swap nibbles in cpu->C
        reset_flags();
        cpu->C = (((cpu->C & 0xF0) >> 4) & 0x0F) | (((cpu->C & 0x0F) << 4 ) & 0xF0);
        if(!cpu->C)set_zero();
        break;
    case 0x32://swap nibbles in cpu->D
        reset_flags();
        cpu->D = (((cpu->D & 0xF0) >> 4) & 0x0F) | (((cpu->D & 0x0F) << 4 ) & 0xF0);
        if(!cpu->D)set_zero();
        break;
    case 0x33://swap nibbles in E
        reset_flags();
        cpu->E = (((cpu->E & 0xF0) >> 4) & 0x0F) | (((cpu->E & 0x0F) << 4 ) & 0xF0);
        if(!cpu->E)set_zero();
        break;
    case 0x34://swap nibbles in cpu->H
        reset_flags();
        cpu->H = (((cpu->H & 0xF0) >> 4) & 0x0F) | (((cpu->H & 0x0F) << 4 ) & 0xF0);
        if(!cpu->H)set_zero();
        break;
    case 0x35://swap nibbles in cpu->L
        reset_flags();
        cpu->L = (((cpu->L & 0xF0) >> 4) & 0x0F) | (((cpu->L & 0x0F) << 4 ) & 0xF0);
        if(!cpu->L)set_zero();
        break;
    case 0x36://swap nibbles in memory at cpu->Hcpu->L
        reset_flags();
        set_mem(u8_to_u16(cpu->H,cpu->L),
	      ((read(u8_to_u16(cpu->H,
			       cpu->L)) & 0xF0) >> 4 | (read(u8_to_u16(cpu->H,cpu->L)) & 0x0F) << 4) & 0xFF);
        if(!get_mem(u8_to_u16(cpu->H,cpu->L))) set_zero();
        break;
    case 0x37://swap nibbles in cpu->A
        reset_flags();
        cpu->A = (((cpu->A & 0xF0) >> 4) & 0x0F) | (((cpu->A & 0x0F) << 4 ) & 0xF0);
        if(!cpu->A)set_zero();
        break;
    case 0x38://shift b right
        reset_flags();
        if(cpu->B & 1)
            set_carry();
        cpu->B = (cpu->B >> 1) & 0xFF;
        if(!cpu->B)
            set_zero();
        break;
    case 0x39://shift cpu->C right
        reset_flags();
        if(cpu->C & 1)
            set_carry();
        cpu->C = (cpu->C >> 1) & 0xFF;
        if(!cpu->C)
            set_zero();
        break;
    case 0x3A://shift cpu->D right
        reset_flags();
        if(cpu->D & 1)
            set_carry();
        cpu->D = (cpu->D >> 1) & 0xFF;
        if(!cpu->D)
            set_zero();
        break;
    case 0x3B://shift E right
        reset_flags();
        if(cpu->E & 1)
            set_carry();
        cpu->E = (cpu->E >> 1) & 0xFF;
        if(!cpu->E)
            set_zero();
        break;
    case 0x3C://shift cpu->H right
        reset_flags();
        if(cpu->H & 1)
            set_carry();
        cpu->H = (cpu->H >> 1) & 0xFF;
        if(!cpu->H)
            set_zero();
        break;
    case 0x3D://shift cpu->L right
        reset_flags();
        if(cpu->L & 1)
            set_carry();
        cpu->L = (cpu->L >> 1) & 0xFF;
        if(!cpu->L)
            set_zero();
        break;
    case 0x3E://shift memory at HL right
        reset_flags();
        tmp_address = u8_to_u16(cpu->H,cpu->L);
        if(get_mem(tmp_address) & 1)
            set_carry();
        write(tmp_address, (read(tmp_address) >> 1) & 0xFF);
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x3F://shift cpu->A right
        reset_flags();
        if(cpu->A & 1)
            set_carry();
        cpu->A = (cpu->A >> 1) & 0xFF;
        if(!cpu->A)
            set_zero();
        break;

    case 0x40://Test bit 0 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x41://Test bit 0 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x42://Test bit 0 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x43://Test bit 0 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x44://Test bit 0 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x45://Test bit 0 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x46://Test bit 0 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x01)) set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x47://Test bit 0 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x48://Test bit 1 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x49://Test bit 1 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4A://Test bit 1 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4B://Test bit 1 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4C://Test bit 1 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4D://Test bit 1 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4E://Test bit 1 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x02))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x4F://Test bit 1 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x02))set_zero();
        else unset_zero();
        break;

    case 0x50://Test bit 2 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x51://Test bit 2 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x52://Test bit 2 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x53://Test bit 2 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x54://Test bit 2 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x55://Test bit 2 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x56://Test bit 2 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x04))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x57://Test bit 2 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x58://Test bit 3 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x59://Test bit 3 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5A://Test bit 3 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5B://Test bit 3 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5C://Test bit 3 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5D://Test bit 3 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5E://Test bit 3 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x08))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x5F://Test bit 3 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x08))set_zero();
        else unset_zero();
        break;

    case 0x60://Test bit 4 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x61://Test bit 4 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x62://Test bit 4 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x63://Test bit 4 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x64://Test bit 4 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x65://Test bit 4 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x66://Test bit 4 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x10))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x67://Test bit 4 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x68://Test bit 5 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x69://Test bit 5 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6A://Test bit 5 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6B://Test bit 5 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6C://Test bit 5 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6D://Test bit 5 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6E://Test bit 5 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x20))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x6F://Test bit 5 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x20))set_zero();
        else unset_zero();
        break;

    case 0x70://Test bit 6 of cpu->B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x71://Test bit 6 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x72://Test bit 6 of D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x73://Test bit 6 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x74://Test bit 6 of H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x75://Test bit 6 of L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x76://Test bit 6 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x40))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x77://Test bit 6 of A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x78://Test bit 7 of B
        set_halfcarry();
        unset_subtract();
        if(!(cpu->B & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x79://Test bit 7 of cpu->C
        set_halfcarry();
        unset_subtract();
        if(!(cpu->C & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7A://Test bit 7 of cpu->D
        set_halfcarry();
        unset_subtract();
        if(!(cpu->D & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7B://Test bit 7 of E
        set_halfcarry();
        unset_subtract();
        if(!(cpu->E & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7C://Test bit 7 of cpu->H
        set_halfcarry();
        unset_subtract();
        if(!(cpu->H & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7D://Test bit 7 of cpu->L
        set_halfcarry();
        unset_subtract();
        if(!(cpu->L & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7E://Test bit 7 of value pointed to by cpu->Hcpu->L
        set_halfcarry();
        unset_subtract();
        if(!(read(u8_to_u16(cpu->H,cpu->L)) & 0x80))set_zero();
        else unset_zero();
	cpu->cycle_counter += 4;
        break;
    case 0x7F://Test bit 7 of cpu->A
        set_halfcarry();
        unset_subtract();
        if(!(cpu->A & 0x80))set_zero();
        else unset_zero();
        break;

    case 0x80://Clear bit 0 of B
        cpu->B &= 0xFE;
        break;
    case 0x81://Clear bit 0 of C
        cpu->C &= 0xFE;
        break;
    case 0x82://Clear bit 0 of D
        cpu->D &= 0xFE;
        break;
    case 0x83://Clear bit 0 of E
        cpu->E &= 0xFE;
        break;
    case 0x84://Clear bit 0 of H
        cpu->H &= 0xFE;
        break;
    case 0x85://Clear bit 0 of L
        cpu->L &= 0xFE;
        break;
    case 0x86://Clear bit 0 of address at HL
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xFE);
        break;
    case 0x87://Clear bit 0 of A
        cpu->A &= 0xFE;
        break;
    case 0x88://Clear bit 1 of B
        cpu->B &= 0xFD;
        break;
    case 0x89://Clear bit 1 of C
        cpu->C &= 0xFD;
        break;
    case 0x8A://Clear bit 1 of D
        cpu->D &= 0xFD;
        break;
    case 0x8B://Clear bit 1 of E
        cpu->E &= 0xFD;
        break;
    case 0x8C://Clear bit 1 of H
        cpu->H &= 0xFD;
        break;
    case 0x8D://Clear bit 1 of L
        cpu->L &= 0xFD;
        break;
    case 0x8E://Clear bit 1 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xFD);
        break;
    case 0x8F://Clear bit 1 of cpu->A
        cpu->A &= 0xFD;
        break;

    case 0x90://Clear bit 2 of cpu->B
        cpu->B &= 0xFB;
        break;
    case 0x91://Clear bit 2 of cpu->C
        cpu->C &= 0xFB;
        break;
    case 0x92://Clear bit 2 of cpu->D
        cpu->D &= 0xFB;
        break;
    case 0x93://Clear bit 2 of E
        cpu->E &= 0xFB;
        break;
    case 0x94://Clear bit 2 of cpu->H
        cpu->H &= 0xFB;
        break;
    case 0x95://Clear bit 2 of cpu->L
        cpu->L &= 0xFB;
        break;
    case 0x96://Clear bit 2 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xFB);
        break;
    case 0x97://Clear bit 2 of cpu->A
        cpu->A &= 0xFB;
        break;
    case 0x98://Clear bit 3 of cpu->B
        cpu->B &= 0xF7;
        break;
    case 0x99://Clear bit 3 of cpu->C
        cpu->C &= 0xF7;
        break;
    case 0x9A://Clear bit 3 of cpu->D
        cpu->D &= 0xF7;
        break;
    case 0x9B://Clear bit 3 of E
        cpu->E &= 0xF7;
        break;
    case 0x9C://Clear bit 3 of cpu->H
        cpu->H &= 0xF7;
        break;
    case 0x9D://Clear bit 3 of cpu->L
        cpu->L &= 0xF7;
        break;
    case 0x9E://Clear bit 3 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xF7);
        break;
    case 0x9F://Clear bit 3 of cpu->A
        cpu->A &= 0xF7;
        break;

    case 0xA0://Clear bit 4 of cpu->B
        cpu->B &= 0xEF;
        break;
    case 0xA1://Clear bit 4 of cpu->C
        cpu->C &= 0xEF;
        break;
    case 0xA2://Clear bit 4 of cpu->D
        cpu->D &= 0xEF;
        break;
    case 0xA3://Clear bit 4 of E
        cpu->E &= 0xEF;
        break;
    case 0xA4://Clear bit 4 of cpu->H
        cpu->H &= 0xEF;
        break;
    case 0xA5://Clear bit 4 of cpu->L
        cpu->L &= 0xEF;
        break;
    case 0xA6://Clear bit 4 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xEF);
        break;
    case 0xA7://Clear bit 4 of cpu->A
        cpu->A &= 0xEF;
        break;
    case 0xA8://Clear bit 5 of cpu->B
        cpu->B &= 0xDF;
        break;
    case 0xA9://Clear bit 5 of cpu->C
        cpu->C &= 0xDF;
        break;
    case 0xAA://Clear bit 5 of cpu->D
        cpu->D &= 0xDF;
        break;
    case 0xAB://Clear bit 5 of E
        cpu->E &= 0xDF;
        break;
    case 0xAC://Clear bit 5 of cpu->H
        cpu->H &= 0xDF;
        break;
    case 0xAD://Clear bit 5 of cpu->L
        cpu->L &= 0xDF;
        break;
    case 0xAE://Clear bit 5 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xDF);
        break;
    case 0xAF://Clear bit 5 of cpu->A
        cpu->A &= 0xDF;
        break;

    case 0xB0://Clear bit 6 of cpu->B
        cpu->B &= 0xBF;
        break;
    case 0xB1://Clear bit 6 of cpu->C
        cpu->C &= 0xBF;
        break;
    case 0xB2://Clear bit 6 of cpu->D
        cpu->D &= 0xBF;
        break;
    case 0xB3://Clear bit 6 of E
        cpu->E &= 0xBF;
        break;
    case 0xB4://Clear bit 6 of cpu->H
        cpu->H &= 0xBF;
        break;
    case 0xB5://Clear bit 6 of cpu->L
        cpu->L &= 0xBF;
        break;
    case 0xB6://Clear bit 6 of address at HL
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0xBF);
        break;
    case 0xB7://Clear bit 6 of cpu->A
        cpu->A &= 0xBF;
        break;
    case 0xB8://Clear bit 7 of cpu->B
        cpu->B &= 0x7F;
        break;
    case 0xB9://Clear bit 7 of cpu->C
        cpu->C &= 0x7F;
        break;
    case 0xBA://Clear bit 7 of cpu->D
        cpu->D &= 0x7F;
        break;
    case 0xBB://Clear bit 7 of E
        cpu->E &= 0x7F;
        break;
    case 0xBC://Clear bit 7 of cpu->H
        cpu->H &= 0x7F;
        break;
    case 0xBD://Clear bit 7 of cpu->L
        cpu->L &= 0x7F;
        break;
    case 0xBE://Clear bit 7 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) & 0x7F);
        break;
    case 0xBF://Clear bit 7 of cpu->A
        cpu->A &= 0x7F;
        break;

    case 0xC0://Set bit 0 of cpu->B
        cpu->B |= 0x01;
        break;
    case 0xC1://Set bit 0 of cpu->C
        cpu->C |= 0x01;
        break;
    case 0xC2://Set bit 0 of cpu->D
        cpu->D |= 0x01;
        break;
    case 0xC3://Set bit 0 of E
        cpu->E |= 0x01;
        break;
    case 0xC4://Set bit 0 of cpu->H
        cpu->H |= 0x01;
        break;
    case 0xC5://Set bit 0 of cpu->L
        cpu->L |= 0x01;
        break;
    case 0xC6://Set bit 0 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x01);
        break;
    case 0xC7://Set bit 0 of cpu->A
        cpu->A |= 0x01;
        break;
    case 0xC8://Set bit 1 of cpu->B
        cpu->B |= 0x02;
        break;
    case 0xC9://Set bit 1 of cpu->C
        cpu->C |= 0x02;
        break;
    case 0xCA://Set bit 1 of cpu->D
        cpu->D |= 0x02;
        break;
    case 0xCB://Set bit 1 of E
        cpu->E |= 0x02;
        break;
    case 0xCC://Set bit 1 of cpu->H
        cpu->H |= 0x02;
        break;
    case 0xCD://Set bit 1 of cpu->L
        cpu->L |= 0x02;
        break;
    case 0xCE://Set bit 1 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x02);
        break;
    case 0xCF://Set bit 1 of cpu->A
        cpu->A |= 0x02;
        break;

    case 0xD0://Set bit 2 of cpu->B
        cpu->B |= 0x04;
        break;
    case 0xD1://Set bit 2 of cpu->C
        cpu->C |= 0x04;
        break;
    case 0xD2://Set bit 2 of cpu->D
        cpu->D |= 0x04;
        break;
    case 0xD3://Set bit 2 of E
        cpu->E |= 0x04;
        break;
    case 0xD4://Set bit 2 of cpu->H
        cpu->H |= 0x04;
        break;
    case 0xD5://Set bit 2 of cpu->L
        cpu->L |= 0x04;
        break;
    case 0xD6://Set bit 2 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x04);
        break;
    case 0xD7://Set bit 2 of cpu->A
        cpu->A |= 0x04;
        break;
    case 0xD8://Set bit 3 of cpu->B
        cpu->B |= 0x08;
        break;
    case 0xD9://Set bit 3 of cpu->C
        cpu->C |= 0x08;
        break;
    case 0xDA://Set bit 3 of cpu->D
        cpu->D |= 0x08;
        break;
    case 0xDB://Set bit 3 of E
        cpu->E |= 0x08;
        break;
    case 0xDC://Set bit 3 of cpu->H
        cpu->H |= 0x08;
        break;
    case 0xDD://Set bit 3 of cpu->L
        cpu->L |= 0x08;
        break;
    case 0xDE://Set bit 3 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x08);
        break;
    case 0xDF://Set bit 3 of cpu->A
        cpu->A |= 0x08;
        break;

    case 0xE0://Set bit 4 of cpu->B
        cpu->B |= 0x10;
        break;
    case 0xE1://Set bit 4 of cpu->C
        cpu->C |= 0x10;
        break;
    case 0xE2://Set bit 4 of cpu->D
        cpu->D |= 0x10;
        break;
    case 0xE3://Set bit 4 of E
        cpu->E |= 0x10;
        break;
    case 0xE4://Set bit 4 of cpu->H
        cpu->H |= 0x10;
        break;
    case 0xE5://Set bit 4 of cpu->L
        cpu->L |= 0x10;
        break;
    case 0xE6://Set bit 4 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x10);
        break;
    case 0xE7://Set bit 4 of cpu->A
        cpu->A |= 0x10;
        break;
    case 0xE8://Set bit 5 of cpu->B
        cpu->B |= 0x20;
        break;
    case 0xE9://Set bit 5 of cpu->C
        cpu->C |= 0x20;
        break;
    case 0xEA://Set bit 5 of cpu->D
        cpu->D |= 0x20;
        break;
    case 0xEB://Set bit 5 of E
        cpu->E |= 0x20;
        break;
    case 0xEC://Set bit 5 of cpu->H
        cpu->H |= 0x20;
        break;
    case 0xED://Set bit 5 of cpu->L
        cpu->L |= 0x20;
        break;
    case 0xEE://Set bit 5 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x20);
        break;
    case 0xEF://Set bit 5 of cpu->A
        cpu->A |= 0x20;
        break;

    case 0xF0://Set bit 6 of cpu->B
        cpu->B |= 0x40;
        break;
    case 0xF1://Set bit 6 of cpu->C
        cpu->C |= 0x40;
        break;
    case 0xF2://Set bit 6 of cpu->D
        cpu->D |= 0x40;
        break;
    case 0xF3://Set bit 6 of E
        cpu->E |= 0x40;
        break;
    case 0xF4://Set bit 6 of cpu->H
        cpu->H |= 0x40;
        break;
    case 0xF5://Set bit 6 of cpu->L
        cpu->L |= 0x40;
        break;
    case 0xF6://Set bit 6 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x40);
        break;
    case 0xF7://Set bit 6 of cpu->A
        cpu->A |= 0x40;
        break;
    case 0xF8://Set bit 7 of cpu->B
        cpu->B |= 0x80;
        break;
    case 0xF9://Set bit 7 of cpu->C
        cpu->C |= 0x80;
        break;
    case 0xFA://Set bit 7 of cpu->D
        cpu->D |= 0x80;
        break;
    case 0xFB://Set bit 7 of E
        cpu->E |= 0x80;
        break;
    case 0xFC://Set bit 7 of cpu->H
        cpu->H |= 0x80;
        break;
    case 0xFD://Set bit 7 of cpu->L
        cpu->L |= 0x80;
        break;
    case 0xFE://Set bit 7 of address at cpu->Hcpu->L
        write(u8_to_u16(cpu->H,cpu->L),read(u8_to_u16(cpu->H,cpu->L)) | 0x80);
        break;
    case 0xFF://Set bit 7 of cpu->A
        cpu->A |= 0x80;
        break;

    default:
        printf("cpu->Ccpu->B opcode 0x%X not implemented yet\n",opcode);
        break;
    }
}

void cpu_writeout_state(u8 opcode)
{
    printf("%X %X %X %X %X %X %X %X %X %X\n",opcode, cpu->A, cpu->B, cpu->C, cpu->D, cpu->E, cpu->H, cpu->L, cpu->SP, cpu->PC);
}

void cpu_step(u8 opcode)
{
    u16 tmp;
    int signed_tmp;
    if(memory->debug)
	printf("op: %X\n", opcode);
    //cpu_writeout_state(opcode);
    switch(opcode) {
    case 0x00://no-op
        break;
    case 0x01://load 16bit immediate into BC
        cpu->C=read(cpu->PC);
        cpu->B=read(cpu->PC + 1);
	cpu->PC += 2;
        break;
    case 0x02://Save A to address pointed by BC
        write(u8_to_u16(cpu->B,cpu->C),cpu->A);
        break;
    case 0x03: // INC BC
	tmp = inc_16(u8_to_u16(cpu->B,cpu->C));
        cpu->B = (tmp >> 8) & 0xFF;
        cpu->C = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x04://INC B
        cpu->B = inc_8(cpu->B);
        break;
    case 0x05://DEC B
        cpu->B = dec_8(cpu->B);
        break;
    case 0x06://Load immediate into B
        cpu->B = read(cpu->PC);
	cpu->PC++;
        break;
    case 0x07:// rotate left through carry accumulator
        cpu->A = rot_left_carry_8(cpu->A);
        break;
    case 0x08://save sp to a given address
    {
	u16 address = (read(cpu->PC+1) << 8) | read(cpu->PC);
        write(address, (cpu->SP & 0xFF));
        write((address + 1) & 0xFFFF, (cpu->SP >> 8) & 0xFF);
        cpu->PC += 2;
        break;
    }
    case 0x09://Add BC to HL
        tmp = add_16(u8_to_u16(cpu->B,cpu->C),u8_to_u16(cpu->H,cpu->L));
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x0A://Load A from addres pointed to by BC
        cpu->A = read(u8_to_u16(cpu->B,cpu->C));
        break;
    case 0x0B://Dec BC
        tmp = dec_16(u8_to_u16(cpu->B,cpu->C));
        cpu->B = (tmp >> 8) & 0xFF;
        cpu->C = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x0C://INC C
        cpu->C = inc_8(cpu->C);
        break;
    case 0x0D://Dec C
        cpu->C = dec_8(cpu->C);
        break;
    case 0x0E://load 8-bit immediate into C
        cpu->C = read(cpu->PC);
	cpu->PC++;
        break;
    case 0x0F://rotate right carry accumulator
        cpu->A = rot_right_carry_8(cpu->A);
        break;

    case 0x10://Stop cpu and lcd until a button is pressed
        printf("Processor stopping\n");
        cpu->cpu_stop = 1;
        break;
    case 0x11://load 16bit immediate into DE
        cpu->E = read(cpu->PC);
        cpu->D = read(cpu->PC + 1);
	cpu->PC += 2;
        break;
    case 0x12://Save A to address pointed by DE
        write(u8_to_u16(cpu->D, cpu->E),cpu->A);
        break;
    case 0x13: //INC DE
        tmp = inc_16(u8_to_u16(cpu->D,cpu->E));
        cpu->D = (tmp >> 8) & 0xFF;
        cpu->E = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x14://INC D
        cpu->D = inc_8(cpu->D);
        break;
    case 0x15://Dec D
        cpu->D = dec_8(cpu->D);
        break;
    case 0x16://Load immediate into D
        cpu->D = read(cpu->PC);
	cpu->PC++;
        break;
    case 0x17://Rotate accumulator left
        cpu->A = rot_left_8(cpu->A);
        break;
    case 0x18://Relative jump by signed immediate
	rjsi();
        break;
    case 0x19://Add DE to HL
        tmp = add_16(u8_to_u16(cpu->D, cpu->E),u8_to_u16(cpu->H,cpu->L));
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x1A://Load A from addres pointed to by DE
        cpu->A = read(u8_to_u16(cpu->D, cpu->E));
        break;
    case 0x1B://Dec DE
        tmp = dec_16(u8_to_u16(cpu->D, cpu->E));
        cpu->D = (tmp >> 8) & 0xFF;
        cpu->E = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x1C://INC E
        cpu->E = inc_8(cpu->E);
        break;
    case 0x1D://Dec E
        cpu->E = dec_8(cpu->E);
        break;
    case 0x1E://load 8-bit immediate into E
        cpu->E = pc_read();
        break;
    case 0x1F://rotate accumulator right
        cpu->A = rot_right_8(cpu->A);
        break;

    case 0x20://Relative jump by signed immediate if last result was not zero
        if(!(cpu->F & 0x80)) {
	    cpu->jump_taken = 1;
	    rjsi();
        } else {
	    pc_change((cpu->PC + 1) & 0xFFFF);
	}
        break;
    case 0x21://load 16bit immediate into cpu->Hcpu->L
        cpu->L = read(cpu->PC);
        cpu->H = read(cpu->PC + 1);
	cpu->PC += 2;
        break;
    case 0x22://Save A to address pointed by cpu->Hcpu->L and increment HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->A);
        tmp = inc_16(u8_to_u16(cpu->H,cpu->L));
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = tmp & 0xFF;
        break;
    case 0x23: //INC HL
        tmp = inc_16(u8_to_u16(cpu->H,cpu->L));
        cpu->H = ((tmp & 0xFF00) >> 8);
        cpu->L = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x24://INC H
        cpu->H = inc_8(cpu->H);
        break;
    case 0x25://Dec H
        cpu->H = dec_8(cpu->H);
        break;
    case 0x26://Load immediate into H
        cpu->H = pc_read();
        break;
    case 0x27:
    { // DAA
        unsigned int a = cpu->A;
        if(!(cpu->F & 0x40)) {
            if((cpu->F & 0x20) || (a & 0x0F) > 9)
                a += 0x06;
            if((cpu->F & 0x10) || (a > 0x9F))
                a += 0x60;
        } else {
            if(cpu->F & 0x20)
                a = (a - 6) & 0xFF;
            if(cpu->F & 0x10)
                a -= 0x60;
        }
        unset_halfcarry();

        if((a & 0x100))
            set_carry();

        a &= 0xFF;

	a == 0 ? set_zero() : unset_zero();
        cpu->A = (u8)a;
    }
    break;
    case 0x28://Relative jump by signed immediate if last result caused a zero
        signed_tmp = (signed char)read(cpu->PC);
        if(cpu->F & 0x80) {
            cpu->PC += signed_tmp;
	    cpu->jump_taken = 1;
	    cpu->cycle_counter += 4;
        }
        cpu->PC++;
        break;
    case 0x29://Add HL to HL
        tmp = add_16(u8_to_u16(cpu->H, cpu->L), u8_to_u16(cpu->H, cpu->L));
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x2A://Load A from address pointed to by HL and INC HL
        cpu->A = read(u8_to_u16(cpu->H, cpu->L));
        tmp = inc_16(u8_to_u16(cpu->H, cpu->L));
        cpu->H = (tmp >> 8);
        cpu->L = tmp & 0xFF;
        break;
    case 0x2B://DEC HL
        tmp = dec_16(u8_to_u16(cpu->H, cpu->L));
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = tmp & 0xFF;
	cpu->cycle_counter += 4;
        break;
    case 0x2C://INC L
        cpu->L = inc_8(cpu->L);
        break;
    case 0x2D://DEC L
        cpu->L = dec_8(cpu->L);
        break;
    case 0x2E://load 8-bit immediate into L
        cpu->L = pc_read();
        break;
    case 0x2F://NOT A/complement of A
        cpu->A = ~cpu->A;
        set_subtract();
        set_halfcarry();
        break;

    case 0x30://Relative jump by signed immediate if last result was not carry
        if(!(cpu->F & 0x10)) {
	    rjsi();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 1) & 0xFFFF);
	}
        break;
    case 0x31://load 16bit immediate into SP
        cpu->SP = (read(cpu->PC + 1) << 8) | read(cpu->PC);
        cpu->PC += 2;
        break;
    case 0x32:{//Save A to address pointed by HL and dec HL
        tmp = dec_16(u8_to_u16(cpu->H, cpu->L));
        write(u8_to_u16(cpu->H, cpu->L), cpu->A);
        cpu->H = (tmp >> 8) & 0xFF;
        cpu->L = (tmp & 0xFF);
        break;
    }
    case 0x33: //INC SP
        cpu->SP = inc_16(cpu->SP);
	cpu->cycle_counter += 4;
        break;
    case 0x34://INC (HL)
        write(u8_to_u16(cpu->H, cpu->L), inc_8(read(u8_to_u16(cpu->H, cpu->L))));
        break;
    case 0x35: //DEC (HL)
        write(u8_to_u16(cpu->H,cpu->L), dec_8(read(u8_to_u16(cpu->H,cpu->L))));
	break;
    case 0x36://Load immediate into address pointed by HL
        write(u8_to_u16(cpu->H,cpu->L), pc_read());
        break;
    case 0x37://set carry flag
        unset_subtract();
        unset_halfcarry();
        set_carry();
        break;
    case 0x38://Relative jump by signed immediate if last result caused a carry
        if(cpu->F & 0x10) {
	    rjsi();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 1) & 0xFFFF);
	}
        break;
    case 0x39://Add SP to HL
    {
	u16 value = add_16(cpu->SP, u8_to_u16(cpu->H, cpu->L));
        cpu->H = (value >> 8) & 0xFF;
        cpu->L = value & 0xFF;
	cpu->cycle_counter += 4;
        break;
    }
    case 0x3A://Load A from addres pointed to by HL and DEC HL
    {
        cpu->A = read(u8_to_u16(cpu->H, cpu->L));
        u16 value = dec_16(u8_to_u16(cpu->H, cpu->L));
        cpu->H = (value >> 8) & 0xFF;
        cpu->L = value & 0xFF;
        break;
    }
    case 0x3B://Dec SP
        cpu->SP = dec_16(cpu->SP);
	cpu->cycle_counter += 4;
        break;
    case 0x3C://Inc A
        cpu->A = inc_8(cpu->A);
        break;
    case 0x3D://Dec A
        cpu->A = dec_8(cpu->A);
        break;
    case 0x3E://load 8-bit immediate into A
        cpu->A = pc_read();
        break;
    case 0x3F://Complement carry flag
        unset_subtract();
        unset_halfcarry();
        if(cpu->F & 0x10)
            unset_carry();
        else
            set_carry();
        break;

    case 0x40://Copy B to B
        break;
    case 0x41://Copy C to B
        cpu->B = cpu->C;
        break;
    case 0x42://Copy D to B
        cpu->B = cpu->D;
        break;
    case 0x43://Copy E to B
        cpu->B = cpu->E;
        break;
    case 0x44://Copy H to B
        cpu->B = cpu->H;
        break;
    case 0x45://Copy L to B
        cpu->B = cpu->L;
        break;
    case 0x46://Copy value pointed by HL to B
        cpu->B = read(u8_to_u16(cpu->H, cpu->L));
        break;
    case 0x47://Copy A to B
        cpu->B = cpu->A;
        break;
    case 0x48://Copy B to C
        cpu->C = cpu->B;
        break;
    case 0x49://Copy C to C
        break;
    case 0x4A://Copy D to C
        cpu->C = cpu->D;
        break;
    case 0x4B://Copy E to C
        cpu->C = cpu->E;
        break;
    case 0x4C://Copy H to C
        cpu->C = cpu->H;
        break;
    case 0x4D://Copy L to C
        cpu->C = cpu->L;
        break;
    case 0x4E://Copy value pointed by HL to C
        cpu->C = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x4F://Copy A to C
        cpu->C = cpu->A;
        break;

    case 0x50://Copy B to D
        cpu->D = cpu->B;
        break;
    case 0x51://Copy C to D
        cpu->D = cpu->C;
        break;
    case 0x52://Copy D to D
        break;
    case 0x53://Copy E to D
        cpu->D = cpu->E;
        break;
    case 0x54://Copy H to D
        cpu->D = cpu->H;
        break;
    case 0x55://Copy L to D
        cpu->D = cpu->L;
        break;
    case 0x56://Copy value pointed by HL to D
        cpu->D = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x57://Copy A to D
        cpu->D = cpu->A;
        break;
    case 0x58://Copy B to E
        cpu->E = cpu->B;
        break;
    case 0x59://Copy C to E
        cpu->E = cpu->C;
        break;
    case 0x5A://Copy D to E
        cpu->E = cpu->D;
        break;
    case 0x5B://Copy E to E
        break;
    case 0x5C://Copy H to E
        cpu->E = cpu->H;
        break;
    case 0x5D://Copy L to E
        cpu->E = cpu->L;
        break;
    case 0x5E://Copy value pointed by HL to E
        cpu->E = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x5F://Copy A to E
        cpu->E = cpu->A;
        break;

    case 0x60://Copy B to H
        cpu->H = cpu->B;
        break;
    case 0x61://Copy C to H
        cpu->H = cpu->C;
        break;
    case 0x62://Copy D to H
        cpu->H = cpu->D;
        break;
    case 0x63://Copy E to H
        cpu->H = cpu->E;
        break;
    case 0x64://Copy H to H
        break;
    case 0x65://Copy L to H
        cpu->H = cpu->L;
        break;
    case 0x66://Copy value pointed by HL to H
        cpu->H = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x67://Copy A to H
        cpu->H = cpu->A;
        break;
    case 0x68://Copy B to L
        cpu->L = cpu->B;
        break;
    case 0x69://Copy C to L
        cpu->L = cpu->C;
        break;
    case 0x6A://Copy D to L
        cpu->L = cpu->D;
        break;
    case 0x6B://Copy E to L
        cpu->L = cpu->E;
        break;
    case 0x6C://Copy H to L
        cpu->L = cpu->H;
        break;
    case 0x6D://Copy L to L
        break;
    case 0x6E://Copy value pointed by HL to L
        cpu->L = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x6F://Copy A to cpu->L
        cpu->L = cpu->A;
        break;

    case 0x70://copy B to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->B);
        break;
    case 0x71://copy C to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->C);
        break;
    case 0x72://copy D to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->D);
        break;
    case 0x73://copy E to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L), cpu->E);
        break;
    case 0x74://copy H to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->H);
        break;
    case 0x75://copy L to address pointed to by HL
        write(u8_to_u16(cpu->H,cpu->L),cpu->L);
        break;
    case 0x76://HALT has bugz on the gb
	if(cpu->interrupt_master_enable)
	    cpu->cpu_halt = 1;
	else {
	    if(memory->interrupt_flags == 0)
	    {
		cpu->cpu_halt = 1;
		cpu->interrupt_skip = 1;
	    }else{
		// HALT bug
		cpu->PC_skip = 1;
	    }
	}
	// Turn interrupts on if they aren't already
        cpu->cpu_halt = 1;
        break;
    case 0x77://copy A to Address pointed to by HL
        write(u8_to_u16(cpu->H, cpu->L), cpu->A);
        break;
    case 0x78:// copy cpu->B to cpu->A
        cpu->A=cpu->B;
        break;
    case 0x79:// copy cpu->C to cpu->A
        cpu->A=cpu->C;
        break;
    case 0x7A:// copy cpu->D to cpu->A
        cpu->A=cpu->D;
        break;
    case 0x7B:// copy E to cpu->A
        cpu->A = cpu->E;
        break;
    case 0x7C:// copy H to A
        cpu->A = cpu->H;
        break;
    case 0x7D:// copy L to A
        cpu->A = cpu->L;
        break;
    case 0x7E://Copy value pointed by HL to A
        cpu->A = read(u8_to_u16(cpu->H,cpu->L));
        break;
    case 0x7F:// copy A to A
        break;

    case 0x80://Acpu->Dcpu->D cpu->B to cpu->A
        cpu->A = add_8(cpu->B,cpu->A);
        break;
    case 0x81://Acpu->Dcpu->D cpu->C to cpu->A
        cpu->A = add_8(cpu->C,cpu->A);
        break;
    case 0x82://Acpu->Dcpu->D cpu->D to cpu->A
        cpu->A = add_8(cpu->D,cpu->A);
        break;
    case 0x83://Acpu->Dcpu->D E to cpu->A
        cpu->A = add_8(cpu->E, cpu->A);
        break;
    case 0x84://Acpu->Dcpu->D cpu->H to cpu->A
        cpu->A = add_8(cpu->H,cpu->A);
        break;
    case 0x85://Acpu->Dcpu->D cpu->L to cpu->A
        cpu->A = add_8(cpu->L,cpu->A);
        break;
    case 0x86://ADD value pointed by HL to A
        cpu->A = add_8(read(u8_to_u16(cpu->H,cpu->L)),cpu->A);
        break;
    case 0x87://Acpu->Dcpu->D cpu->A to cpu->A
        cpu->A = add_8(cpu->A,cpu->A);
        break;
    case 0x88://Add cpu->B and carry flag to cpu->A
        cpu->A = add_8c(cpu->A,cpu->B);
        break;
    case 0x89://Add cpu->C and carry flag to cpu->A
        cpu->A = add_8c(cpu->A,cpu->C);
        break;
    case 0x8A://Add cpu->D and carry flag to cpu->A
        cpu->A = add_8c(cpu->A, cpu->D);
        break;
    case 0x8B://Add E and carry flag to cpu->A
        cpu->A = add_8c(cpu->A, cpu->E);
        break;
    case 0x8C://Add cpu->H and carry flag to cpu->A
        cpu->A = add_8c(cpu->A,cpu->H);
        break;
    case 0x8D://Add cpu->L and carry flag to cpu->A
        cpu->A = add_8c(cpu->A,cpu->L);
        break;
    case 0x8E://Add value pointed by HL and carry flag to A
        cpu->A = add_8c(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0x8F://Add A and carry flag to A
        cpu->A = add_8c(cpu->A,cpu->A);
        break;

    case 0x90://SUB B from A
        cpu->A = sub_8(cpu->A,cpu->B);
        break;
    case 0x91://SUB cpu->C from cpu->A
        cpu->A = sub_8(cpu->A,cpu->C);
        break;
    case 0x92://SUB cpu->D from cpu->A
        cpu->A = sub_8(cpu->A,cpu->D);
        break;
    case 0x93://SUB E from cpu->A
        cpu->A = sub_8(cpu->A, cpu->E);
        break;
    case 0x94://SUB cpu->H from cpu->A
        cpu->A = sub_8(cpu->A,cpu->H);
        break;
    case 0x95://SUB cpu->L from cpu->A
        cpu->A = sub_8(cpu->A,cpu->L);
        break;
    case 0x96://SUB value pointed by cpu->Hcpu->L from cpu->A
        cpu->A = sub_8(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0x97://SUB cpu->A from cpu->A
        cpu->A = sub_8(cpu->A,cpu->A);
        break;
    case 0x98://SUB cpu->B and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->B);
        break;
    case 0x99://SUB cpu->C and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->C);
        break;
    case 0x9A://SUB cpu->D and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->D);
        break;
    case 0x9B://SUB E and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A, cpu->E);
        break;
    case 0x9C://SUB cpu->H and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->H);
        break;
    case 0x9D://SUB cpu->L and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->L);
        break;
    case 0x9E://SUB value pointed by cpu->Hcpu->L and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0x9F://SUB cpu->A and carry flag from cpu->A
        cpu->A = sub_8c(cpu->A,cpu->A);
        break;
    case 0xA0://AND cpu->A with cpu->B
        cpu->A = and_8(cpu->A,cpu->B);
        break;
    case 0xA1://AND cpu->A with cpu->C
        cpu->A = and_8(cpu->A,cpu->C);
        break;
    case 0xA2://AND cpu->A with cpu->D
        cpu->A = and_8(cpu->A,cpu->D);
        break;
    case 0xA3://AND A with E
        cpu->A = and_8(cpu->A, cpu->E);
        break;
    case 0xA4://AND A with H
        cpu->A = and_8(cpu->A,cpu->H);
        break;
    case 0xA5://AND A with L
        cpu->A = and_8(cpu->A,cpu->L);
        break;
    case 0xA6://AND A with value pointed to by HL
        cpu->A = and_8(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0xA7://And A with A
        cpu->A = and_8(cpu->A,cpu->A);
        break;
    case 0xA8://XOR B with A
        cpu->A = xor_8(cpu->A,cpu->B);
        break;
    case 0xA9://XOR C with A
        cpu->A = xor_8(cpu->A,cpu->C);
        break;
    case 0xAA://XOR D with A
        cpu->A = xor_8(cpu->A,cpu->D);
        break;
    case 0xAB://XOR E with A
        cpu->A = xor_8(cpu->A, cpu->E);
        break;
    case 0xAC://XOR H with A
        cpu->A = xor_8(cpu->A,cpu->H);
        break;
    case 0xAD://XOR L with A
        cpu->A = xor_8(cpu->A,cpu->L);
        break;
    case 0xAE://XOR A with value pointed to by HL
        cpu->A = xor_8(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0xAF://XOR A with A
        cpu->A = xor_8(cpu->A,cpu->A);
        break;

    case 0xB0://OR A with B
        cpu->A = or_8(cpu->A,cpu->B);
        break;
    case 0xB1://OR A with C
        cpu->A = or_8(cpu->A,cpu->C);
        break;
    case 0xB2://OR A with D
        cpu->A = or_8(cpu->A,cpu->D);
        break;
    case 0xB3://OR A with E
        cpu->A = or_8(cpu->A, cpu->E);
        break;
    case 0xB4://OR A with H
        cpu->A = or_8(cpu->A,cpu->H);
        break;
    case 0xB5://OR A with L
        cpu->A = or_8(cpu->A,cpu->L);
        break;
    case 0xB6://OR A with value pointed by HL
        cpu->A = or_8(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0xB7://OR A with A
        cpu->A = or_8(cpu->A,cpu->A);
        break;
    case 0xB8://compare B against A
        comp_8(cpu->A,cpu->B);
        break;
    case 0xB9://compare C against A
        comp_8(cpu->A,cpu->C);
        break;
    case 0xBA://compare D against A
        comp_8(cpu->A,cpu->D);
        break;
    case 0xBB://compare E against A
        comp_8(cpu->A, cpu->E);
        break;
    case 0xBC://compare H against A
        comp_8(cpu->A,cpu->H);
        break;
    case 0xBD://compare L against A
        comp_8(cpu->A,cpu->L);
        break;
    case 0xBE://compare value pointed by HL against A
        comp_8(cpu->A,read(u8_to_u16(cpu->H,cpu->L)));
        break;
    case 0xBF://compare A against A
        comp_8(cpu->A,cpu->A);
        break;

    case 0xC0://Return if last result was not zero
        if(!(cpu->F & 0x80)) {
	    ret();
	    cpu->jump_taken = 1;
        }
	cpu->cycle_counter += 4;

        break;
    case 0xC1://POP stack into BC
        cpu->C = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        cpu->B = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        break;
    case 0xC2://Absolute jump to 16 bit location if last result not zero
        if(!(cpu->F & 0x80)) {
	    absolute_jump();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xC3://Absolute jump to 16 bit location
	absolute_jump();
        break;
    case 0xC4://call routine at 16 bit immediate if last result not zero
        if(!(cpu->F & 0x80)) {
	    call_nn();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xC5://PUSH BC onto stack
	push_stack(cpu->B, cpu->C);
	cpu->cycle_counter += 4;
        break;
    case 0xC6://ADD immediate to A
    {
        cpu->A = add_8(cpu->A, pc_read());
        break;
    }
    case 0xC7://call routine at 0
	call_routine(0);
        break;
    case 0xC8://Return if last result was zero
        if(cpu->F & 0x80) {
	    ret();
	    cpu->jump_taken = 1;
        }
	cpu->cycle_counter += 4;
        break;
    case 0xC9://Return
	ret();
        break;
    case 0xCA://Absolute jump to 16 bit location if last result zero
        if(cpu->F & 0x80) {
	    absolute_jump();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xCB://double byte instruction
    {
	u8 tmp = pc_read();
	cb_opcodes(tmp);
	u8 cycles = load_json(med_obj, tmp, cpu->jump_taken ? 1 : 0, "cbprefixed");
	if(cpu->cycle_counter != cycles)
	    printf("CB expected %d got %d opcode %X\n", cycles, cpu->cycle_counter, tmp);
    }

    //cb_opcodes(pc_read());
        break;
    case 0xCC://call routine at 16 bit immediate if last result was zero
        if(cpu->F & 0x80) {
	    call_nn();
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xCD://call routine at 16 bit immediate
	call_nn();
        break;
    case 0xCE://ADD immediate and carry to A
        cpu->A = add_8c(cpu->A,pc_read());
        break;
    case 0xCF://call routine at 0x0008
	call_routine(8);
        break;
    case 0xD0://Return if last result was not carry
        if(!(cpu->F & 0x10)) {
	    ret();
	    cpu->jump_taken = 1;
        }
	cpu->cycle_counter += 4;

        break;
    case 0xD1://POP stack into DE
        cpu->E = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        cpu->D = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        break;
    case 0xD2://Absolute jump to 16 bit location if last result not carry
        if(!(cpu->F & 0x10)) {
	    pc_change(u8_to_u16(read(cpu->PC + 1), read(cpu->PC)));
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xD4://call routine at 16 bit immediate if last result not carry
        if(!(cpu->F & 0x10)) {
	    push_stack(((cpu->PC + 2) >> 8) & 0xFF, (cpu->PC + 2) & 0xFF);
	    pc_change(u8_to_u16(read(cpu->PC + 1), read(cpu->PC)));
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xD5://PUSH DE onto stack
	push_stack(cpu->D, cpu->E);
	cpu->cycle_counter += 4;
        break;
    case 0xD6://SUB immediate against A
        cpu->A = sub_8(cpu->A,pc_read());
        break;
    case 0xD7://call routine at 0x10
	call_routine(0x10);
        break;
    case 0xD8://Return if last result was carry
        if(cpu->F & 0x10) {
	    ret();
	    cpu->jump_taken = 1;
        }
	cpu->cycle_counter += 4;
        break;
    case 0xD9://enable interrupts and return
        cpu->interrupt_master_enable = 1;
	ret();
        break;
    case 0xDA://Absolute jump to 16 bit immediate if last result carry
        if(cpu->F & 0x10) {
            pc_change(u8_to_u16(read(cpu->PC + 1),read(cpu->PC)));
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
        break;
    case 0xDB:// not used
        break;
    case 0xDC://call routine at 16 bit immediate if last result carry
        if(cpu->F & 0x10) {
            write(--cpu->SP,((cpu->PC + 2) >> 8) & 0xFF);
            write(--cpu->SP,((cpu->PC + 2) & 0xFF) );
            pc_change(u8_to_u16(read(cpu->PC + 1),read(cpu->PC)));
	    cpu->jump_taken = 1;
        } else {
	    pc_change((cpu->PC + 2) & 0xFFFF);
	    cpu->cycle_counter += 4;
        }
	break;
    case 0xDD://not used
	break;
    case 0xDE://Subtract immediate and carry from A
	cpu->A = sub_8c(cpu->A,pc_read());
        break;
    case 0xDF://call routine at 0x0018
	call_routine(0x18);
        break;

    case 0xE0://save A at address pointed to by 0xFF00 + immediate
        write(0xFF00 + pc_read(),cpu->A);
        break;
    case 0xE1://POP stack into HL
        cpu->L = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        cpu->H = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        break;
    case 0xE2://save A at address pointed to by 0xFF00 + C
        write(0xFF00 + cpu->C,cpu->A);
        break;
    case 0xE3://not used
        break;
    case 0xE4://not used
        break;
    case 0xE5://PUSH HL onto stack
	push_stack(cpu->H, cpu->L);
	cpu->cycle_counter += 4;
        break;
    case 0xE6://AND immediate against cpu->A
        cpu->A = and_8(cpu->A,pc_read());
        break;
    case 0xE7://call routine at 0x20
	call_routine(0x20);
        break;
    case 0xE8: { //add signed 8bit immediate to SP
        int number = (signed char)pc_read();
        int result = cpu->SP + number;
        reset_flags();
        if((cpu->SP ^ number ^ (result & 0xFFFF)) & 0x100)
            set_carry();
        if((cpu->SP ^ number ^ (result & 0xFFFF)) & 0x10)
            set_halfcarry();
        cpu->SP = result & 0xFFFF;
	cpu->cycle_counter += 8;
    }
    break;
    case 0xE9://PC equals HL
	cpu->PC = u8_to_u16(cpu->H,cpu->L);
        break;
    case 0xEA://save A at 16bit immediate given address
        write(u8_to_u16(read(cpu->PC + 1), read(cpu->PC)), cpu->A);
        cpu->PC += 2;
        break;
    case 0xEB://not used
        break;
    case 0xEC://not used
        break;
    case 0xED://not used
        break;
    case 0xEE://xor 8 bit immediate against A
        cpu->A = xor_8(cpu->A,pc_read());
        break;
    case 0xEF://call routine at 0x28
	call_routine(0x28);
        break;

    case 0xF0://load A from address pointed to by 0xFF00 + immediate 8bit
        cpu->A = read(0xFF00 + pc_read());
        break;
    case 0xF1://POP stack into AF low nibbles of flag register should be zeroed
        cpu->F = read(cpu->SP) & 0xF0;
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        cpu->A = read(cpu->SP);
	cpu->SP = (cpu->SP + 1) & 0xFFFF;
        break;
    case 0xF2://Put value at address 0xFF00 + register C into A
        cpu->A = read(0xFF00 + cpu->C);
        break;
    case 0xF3://Disable interrupts.
        cpu->interrupt_master_enable = 0;
        break;
    case 0xF5://PUSH AF onto stack
	push_stack(cpu->A, cpu->F);
	cpu->cycle_counter += 4;
        break;
    case 0xF6://OR immediate against A
        cpu->A = or_8(cpu->A,pc_read());
        break;
    case 0xF7://call routine at 0x30
	call_routine(0x30);
        break;
    case 0xF8:
    { //Add signed immediate to SP and save result in HL
        int number = (signed char)pc_read();
        int result = cpu->SP + number;
        reset_flags();
        if((cpu->SP ^ number ^ (result & 0xFFFF)) & 0x100)
            set_carry();
        if((cpu->SP ^ number ^ (result & 0xFFFF)) & 0x10)
            set_halfcarry();
        cpu->H = (result >> 8) & 0xFF;
        cpu->L = result & 0xFF;
	cpu->cycle_counter += 4;
    }
    break;
    case 0xF9://copy HL to SP
        cpu->SP = u8_to_u16(cpu->H,cpu->L);
	cpu->cycle_counter += 4;
        break;
    case 0xFA://load A from given address
        cpu->A = read(u8_to_u16(read(cpu->PC + 1),read(cpu->PC)));
        cpu->PC += 2;
        break;
    case 0xFB://Enable interrupts.
        cpu->interrupt_master_enable = 1;
        break;
    case 0xFC://not used
        break;
    case 0xFD://not used
        break;
    case 0xFE://compare 8 bit immediate against A
        comp_8(cpu->A, pc_read());
        break;
    case 0xFF://call routine at 0x38
	call_routine(0x38);
        break;

    default:
        printf("%X Not implemented yet\n",opcode);
        break;
    }
}

static void interrupt(u16 address)
{
    cpu->interrupt_master_enable = 0;
    write(--cpu->SP, (cpu->PC >> 8) & 0xFF);
    write(--cpu->SP, (cpu->PC & 0xFF));
    cpu->PC = address;
    timer_tick(12);
    gpu_step(12);
}

void print_cpu()
{
    printf("cpu->A: %X cpu->F: %X cpu->B: %X cpu->C: %X cpu->D: %X E: %X cpu->H: %X cpu->L: %X\n",
	   cpu->A,cpu->F,cpu->B,cpu->C,cpu->D,cpu->E,cpu->H,cpu->L);
    printf("SP: %X\n",cpu->SP);
    printf("cpu->PC:%X\n",cpu->PC);
}

void cpu_init()
{
    cpu = malloc(sizeof(Cpu));
    cpu->PC = 0x100;
    cpu->SP = 0xFFFE;
    cpu->H = 0x01;
    cpu->L = 0x4D;
    cpu->F = 0;
    cpu->E = 0xD8;
    cpu->D = 0;
    cpu->C = 0x13;
    cpu->B = 0;
    cpu->A = 0x01;

    cpu->interrupt_master_enable = 0;
    cpu->cpu_halt = 0;
    cpu->cpu_exit_loop = 0;
    cpu->PC_skip = 0;
    cpu->interrupt_skip = 0;
}

void cpu_exit()
{
    cpu->cpu_exit_loop = 1;
}

void cpu_run_once()
{
    print_cpu();
    cpu_step(pc_read());

    printf("opcode: %X\n",read(cpu->PC-1));
    //gpu_step(cpu->op_time);
    cpu->PC &= 0xFFFF;
    print_cpu();
}

static unsigned int load_json(const json_object *med_obj,
			      const unsigned int opcode, const int pos,
			      const char *prefix)
{
    struct json_object *medi_array, *medj_array;
    medi_array = json_object_object_get(med_obj, prefix);
    char temp_c[4];
    sprintf(temp_c, "0x%x", opcode);
    medi_array = json_object_object_get(medi_array, temp_c);
    medj_array = json_object_object_get(medi_array, "cycles");
    int arraylen = json_object_array_length(medj_array);
    if(arraylen == 1)
	return json_object_get_int(json_object_array_get_idx(medj_array, 0));
    else
	return json_object_get_int(json_object_array_get_idx(medj_array, pos ? 0 : 1));
}

void cpu_run()
{
    static const char filename[] = "opcodes.json";
    med_obj = json_object_from_file(filename);
    if (!med_obj) {
	fprintf(stderr, "load JSON data from %s failed.\n", filename);
	return;
    }


    while(!cpu->cpu_exit_loop) {
	cpu->cycle_counter = 0;
	cpu->jump_taken = 0;
	if(cpu->cpu_halt){
	    cpu->cycle_counter += 4;
	}else{
	    // Read next instruction from PC
	    // but don't increment it
	    if(cpu->PC_skip){
		cpu_step(read(cpu->PC));
		cpu->PC_skip = 0;
	    }
	    else{
		u8 tmp = pc_read();
		cpu_step(tmp);
		u8 cycles = load_json(med_obj, tmp, cpu->jump_taken ? 1 : 0, "unprefixed");
		if(cpu->cycle_counter != cycles && tmp != 0xCB)
		    printf("!!!! expected %d got %d opcode %X\n", cycles, cpu->cycle_counter, tmp);
	    }
	}
	timer_tick(cpu->cycle_counter);
	gpu_step(cpu->cycle_counter);

        if((cpu->interrupt_master_enable || cpu->cpu_halt) && memory->interrupt_enable && memory->interrupt_flags) {
            int fired = memory->interrupt_enable & memory->interrupt_flags;
	    cpu->cpu_halt = 0;
	    if(cpu->interrupt_skip)
		cpu->interrupt_skip = 0;
	    else {
		if(fired & 0x01) { // VBLANK
		    memory->interrupt_flags &= ~0x01;
		    interrupt(0x0040);
		}
		else if(fired & 0x02) { //LCD STAT
		    memory->interrupt_flags &= ~0x02;
		    interrupt(0x0048);
		}
		else if(fired & 0x04) { // TIMER
		    memory->interrupt_flags &= ~0x04;
		    interrupt(0x0050);
		}
		else if(fired & 0x08) { // SERIAL
		    memory->interrupt_flags &= ~0x08;
		    interrupt(0x0058);
		}
		else if(fired & 0x10) { // Joypad
		    memory->interrupt_flags &= ~0x10;
		    interrupt(0x0060);
		}
	    }
        }
    }
}
