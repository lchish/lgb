#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "cpu_timings.h"
#include "types.h"
#include "defs.h"
#include "mem.h"
#include "gpu.h"

//registers
u8 A; //a and b registers
u8 B;
u8 C;
u8 D;
u8 E;
u8 H;
u8 L;
u8 F;//flags
u16 PC;
u16 SP;

//stored registers
u8 sv_A;
u8 sv_B;
u8 sv_C;
u8 sv_D;
u8 sv_E;
u8 sv_H;
u8 sv_L;
u8 sv_F;

//cpu information
int interrupts;
unsigned long cpu_time;
int cpu_halt;
int cpu_stop;
int op_time;
int cpu_exit_loop;

FILE *files;
//convert two u8s to a single u16
static inline u16 u8_to_u16(const u8 high,const u8 low){
    return (u16) (((high << 8) & 0xFF00) + low);
}
//set/unset flags
static inline void set_zero(){
    F |= 0x80;
}
static inline void unset_zero(){
    F &= 0x7F;
}
static inline void set_subtract(){
    F |= 0x40;
}
static inline void unset_subtract(){
    F &= 0xBF;
}
static inline void set_halfcarry(){
    F |=0x20;
}
static inline void unset_halfcarry(){
    F &= 0xDF;
}
static inline void set_carry(){
    F |= 0x10;
}
static inline void unset_carry(){
    F &= 0xEF;
}
static inline void reset_flags(){
    F = 0;
}
//arithmetic operations
static inline u8 inc_8(const u8 to_inc){
    unset_subtract();
    unset_halfcarry();
    unset_zero();
    if((to_inc & 0xF) == 0xF)
        set_halfcarry();
    if(to_inc == 0xFF){
        set_zero();
        return 0;
    }
    return to_inc + 1;
}
static inline u8 dec_8(const u8 to_dec){
    set_subtract();
    unset_halfcarry();
    unset_zero();
    if((to_dec & 0xF) == 0) set_halfcarry();
    if(to_dec == 1){
        set_zero();
        return 0;
    }
    return to_dec == 0 ? 0xFF : to_dec - 1;
}
static inline u8 add_8(const u8 a,const u8 b){
    unsigned int ret = a + b;
    reset_flags();
    if(!(ret & 0xFF)) set_zero();
    if(ret > 0xFF) set_carry();
    if((a & 0xF) + (b & 0xF) > 0xF) set_halfcarry();
    return ret & 0xFF;
}
static inline u8 add_8c(const u8 a,const u8 b){//also add carry flag
    unsigned int carry = (F >> 4) & 1;
    unsigned int ret = a + b + carry;
    reset_flags();
    if(!(ret & 0xFF)) set_zero();
    if(ret > 0xFF) set_carry();
    if((a & 0xF) + (b & 0xF) + carry > 0xF) set_halfcarry();
    return ret & 0xFF;
}
static inline u8 sub_8(const u8 a,const u8 b){
    u8 ret = (a - b) & 0xFF;
    reset_flags();
    set_subtract();
    if(!(ret & 0xFF)) set_zero();
    if(a - b < 0) set_carry();
    if((a & 0xF) - (b & 0xF) < 0) set_halfcarry();
    return ret;
}

static inline u8 sub_8c(const u8 a,const u8 b){
    int carry = (F >> 4) & 1;
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
static inline u16 inc_16(const u16 to_inc){  //sets no flags
    return (to_inc + 1) & 0xFFFF;
}
static inline u16 dec_16(const u16 to_dec){
    return (to_dec - 1) & 0xFFFF;
}
static inline u16 add_16(const u16 a,const u16 b){
    unset_subtract();
    unset_halfcarry();
    unset_carry();
    unsigned int ret = a + b;
    if(ret > 0xFFFF) set_carry();
    if((a & 0x07FF) + (b & 0x07FF) > 0x07FF) set_halfcarry();
    return ret & 0xFFFF;
}
//logic
static inline u8 and_8(const u8 a,const u8 b){
    u8 ret = a & b;
    reset_flags();
    set_halfcarry();
    if(!ret) set_zero();
    return ret;
}
static inline u8 or_8(const u8 a,const u8 b){
    u8 ret = a | b;
    reset_flags();
    if(!ret) set_zero();
    return ret;
}
static inline u8 xor_8(const u8 a,const u8 b){
    u8 ret = a ^ b;
    reset_flags();
    if(!ret) set_zero();
    return ret;
}

static inline u8 rot_left_carry_8(const u8 a){
    reset_flags();
    if(a & 0x80) set_carry();
    return ((a << 1) | (a >> 7)) & 0xFF;
}
static inline u8 rot_left_8(const u8 a){
    u8 ret;
    ret = ((a << 1) | ((F >> 4) & 0x01)) & 0xFF;
    reset_flags();
    if (a & 0x80) set_carry();
    return ret;
}
static inline u8 rot_right_carry_8(const u8 a){
    reset_flags();
    if (a & 0x01) set_carry();
    return ((a >> 1) | (a << 7)) & 0xFF;
}
static inline u8 rot_right_8(const u8 a){
    u8 ret;
    ret = ((a >> 1) | (F & 0x10 ? 0x80 : 0x00)) & 0xFF;
    reset_flags();
    if (a & 0x01) set_carry();
    return ret ;
}
//compare
static inline void comp_8(const u8 a,const u8 b){
    int ret = a - b;
    reset_flags();
    set_subtract();
    if(!ret) set_zero();
    if(ret < 0) set_carry();
    if((a & 0xF) - (b & 0xF ) < 0) set_halfcarry();
}

static inline void store_regs(){
    sv_A = A;
    sv_B = B;
    sv_C = C;
    sv_D = D;
    sv_E = E;
    sv_F = F;
    sv_H = H;
    sv_L = L;
}

static inline void load_regs(){
    A = sv_A;
    B = sv_B;
    C = sv_C;
    D = sv_D;
    E = sv_E;
    F = sv_F;
    H = sv_H;
    L = sv_L;
}

void cb_opcodes(const u8 opcode){
    u16 tmp_address;

    switch(opcode){
    case 0x00://Rotate B left with carry
        B = rot_left_carry_8(B);
	if(!B) set_zero();
        break;
    case 0x01://Rotate C left with carry
        C = rot_left_carry_8(C);
	if(!C) set_zero();
        break;
    case 0x02://Rotate D left with carry
        D = rot_left_carry_8(D);
	if(!D) set_zero();
        break;
    case 0x03://Rotate E left with carry
        E = rot_left_carry_8(E);
	if(!E) set_zero();
        break;
    case 0x04://Rotate H left with carry
        H = rot_left_carry_8(H);
	if(!H) set_zero();
        break;
    case 0x05://Rotate L left with carry
        L = rot_left_carry_8(L);
	if(!L) set_zero();
        break;
    case 0x06://Rotate value pointed by HL left with carry
        tmp_address = u8_to_u16(H,L);
        set_mem(tmp_address,rot_left_carry_8(get_mem(tmp_address)));
	if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x07://Rotate A left with carry
        A = rot_left_carry_8(A);
	if(!A) set_zero();
        break;
    case 0x08://Rotate B right with carry
        B = rot_right_carry_8(B);
	if(!B) set_zero();
        break;
    case 0x09://Rotate C right with carry
        C = rot_right_carry_8(C);
	if(!C) set_zero();
        break;
    case 0x0A://Rotate D right with carry
        D = rot_right_carry_8(D);
	if(!D) set_zero();
        break;
    case 0x0B://Rotate E right with carry
        E = rot_right_carry_8(E);
	if(!E) set_zero();
        break;
    case 0x0C://Rotate H right with carry
        H = rot_right_carry_8(H);
	if(!H) set_zero();
        break;
    case 0x0D://Rotate L right with carry
        L = rot_right_carry_8(L);
	if(!L) set_zero();
        break;
    case 0x0E://Rotate value pointed by HL right with carry
        tmp_address = u8_to_u16(H,L);
        set_mem(tmp_address,rot_right_carry_8(get_mem(tmp_address)));
	if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x0F://Rotate A right with carry
        A = rot_right_carry_8(A);
	if(!A) set_zero();
        break;

    case 0x10://Rotate B left
        B = rot_left_8(B);
        if(!B) set_zero();
        break;
    case 0x11://Rotate C left
        C = rot_left_8(C);
        if(!C) set_zero();
        break;
    case 0x12://Rotate D left
        D = rot_left_8(D);
        if(!D) set_zero();
        break;
    case 0x13://Rotate E left
        E = rot_left_8(E);
        if(!E) set_zero();
        break;
    case 0x14://Rotate H left
        H = rot_left_8(H);
        if(!H) set_zero();
        break;
    case 0x15://Rotate L left
        L = rot_left_8(L);
        if(!L) set_zero();
        break;
    case 0x16://Rotate value pointed by HL left
        tmp_address = u8_to_u16(H,L);
        set_mem(tmp_address,rot_left_8(get_mem(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x17://Rotate A left
        A = rot_left_8(A);
        if(!A) set_zero();
        break;
    case 0x18://Rotate B right
        B = rot_right_8(B);
        if(!B) set_zero();
        break;
    case 0x19://Rotate C right
        C = rot_right_8(C);
        if(!C) set_zero();
        break;
    case 0x1A://Rotate D right
        D = rot_right_8(D);
        if(!D) set_zero();
        break;
    case 0x1B://Rotate E right
        E = rot_right_8(E);
        if(!E) set_zero();
        break;
    case 0x1C://Rotate H right
        H = rot_right_8(H);
        if(!H) set_zero();
        break;
    case 0x1D://Rotate L right
        L = rot_right_8(L);
        if(!L) set_zero();
        break;
    case 0x1E://Rotate value pointed by HL right
        tmp_address = u8_to_u16(H,L);
        set_mem(tmp_address,rot_right_8(get_mem(tmp_address)));
        if(!get_mem(tmp_address)) set_zero();
        break;
    case 0x1F://Rotate A right
        A = rot_right_8(A);
        if(!A) set_zero();
        break;

    case 0x20://Shift B left into carry LSB set to 0
        reset_flags();
        if(B & 0x80)
            set_carry();
        B = (B << 1) & 0xFF;
        if(!B)
            set_zero();
        break;
    case 0x21://Shift C left into carry LSB set to 0
        reset_flags();
        if(C & 0x80)
            set_carry();
        C = (C << 1) & 0xFF;
        if(!C)
            set_zero();
        break;
    case 0x22://Shift D left into carry LSB set to 0
        reset_flags();
        if(D & 0x80)
            set_carry();
        D = (D << 1) & 0xFF;
        if(!D)
            set_zero();
        break;
    case 0x23://Shift E left into carry LSB set to 0
        reset_flags();
        if(E & 0x80)
            set_carry();
        E = (E << 1) & 0xFF;
        if(!E)
            set_zero();
        break;
    case 0x24://Shift H left into carry LSB set to 0
        reset_flags();
        if(H & 0x80)
            set_carry();
        H = (H << 1) & 0xFF;
        if(!H)
            set_zero();
        break;
    case 0x25://Shift L left into carry LSB set to 0
        reset_flags();
        if(L & 0x80)
            set_carry();
        L = (L << 1) & 0xFF;
        if(!L)
            set_zero();
        break;
    case 0x26://Shift value pointed to by HL left into carry LSB set to 0
        reset_flags();
        tmp_address = u8_to_u16(H,L);
        if(get_mem(tmp_address) & 0x80)
            set_carry();
        set_mem(tmp_address, (get_mem(tmp_address) << 1) & 0xFF);
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x27://Shift n left into carry LSB set to 0
        reset_flags();
        if(A & 0x80)
            set_carry();
        A = (A << 1) & 0xFF;
        if(!A)
            set_zero();
        break;
    case 0x28://Shift B right into carry. MSB doesn't change
        reset_flags();
        if(B & 1)
            set_carry();
        B = (B >> 1) | (B & 0x80);
        if(!B)
            set_zero();
        break;
    case 0x29://Shift C right into carry. MSB doesn't change
        reset_flags();
        if(C & 1)
            set_carry();
        C = (C >> 1) | (C & 0x80);
        if(!C)
            set_zero();
        break;
    case 0x2A://Shift D right into carry. MSB doesn't change
        reset_flags();
        if(D & 1)
            set_carry();
        D = (D >> 1) | (D & 0x80);
        if(!D)
            set_zero();
        break;
    case 0x2B://Shift E right into carry. MSB doesn't change
        reset_flags();
        if(E & 1)
            set_carry();
        E = (E >> 1) | (E & 0x80);
        if(!E)
            set_zero();
        break;
    case 0x2C://Shift H right into carry. MSB doesn't change
        reset_flags();
        if(H & 1)
            set_carry();
        H = (H >> 1) | (H & 0x80);
        if(!H)
            set_zero();
        break;
    case 0x2D://Shift L right into carry. MSB doesn't change
        reset_flags();
        if(L & 1)
            set_carry();
        L = (L >> 1) | (L & 0x80);
        if(!L)
            set_zero();
        break;
    case 0x2E://Shift memory at HL right into carry. MSB doesn't change
        reset_flags();
        tmp_address = u8_to_u16(H,L);
        if(get_mem(tmp_address) & 1)
            set_carry();
        set_mem(tmp_address, (get_mem(tmp_address) >> 1) | (get_mem(tmp_address) & 0x80));
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x2F://Shift A right into carry. MSB doesn't change
        reset_flags();
        if(A & 1)
            set_carry();
        A = (A >> 1) | (A & 0x80);
        if(!A)
            set_zero();
        break;

    case 0x30://swap nibbles in B
        reset_flags();
        B = (((B & 0xF0) >> 4) & 0x0F) | (((B & 0x0F) << 4 ) & 0xF0);
        if(!B)set_zero();
        break;
    case 0x31://swap nibbles in C
        reset_flags();
        C = (((C & 0xF0) >> 4) & 0x0F) | (((C & 0x0F) << 4 ) & 0xF0);
        if(!C)set_zero();
        break;
    case 0x32://swap nibbles in D
        reset_flags();
        D = (((D & 0xF0) >> 4) & 0x0F) | (((D & 0x0F) << 4 ) & 0xF0);
        if(!D)set_zero();
        break;
    case 0x33://swap nibbles in E
        reset_flags();
        E = (((E & 0xF0) >> 4) & 0x0F) | (((E & 0x0F) << 4 ) & 0xF0);
        if(!E)set_zero();
        break;
    case 0x34://swap nibbles in H
        reset_flags();
        H = (((H & 0xF0) >> 4) & 0x0F) | (((H & 0x0F) << 4 ) & 0xF0);
        if(!H)set_zero();
        break;
    case 0x35://swap nibbles in L
        reset_flags();
        L = (((L & 0xF0) >> 4) & 0x0F) | (((L & 0x0F) << 4 ) & 0xF0);
        if(!L)set_zero();
        break;
    case 0x36://swap nibbles in memory at HL
        reset_flags();
        set_mem(u8_to_u16(H,L), ((get_mem(u8_to_u16(H,L)) & 0xF0) >> 4 | (get_mem(u8_to_u16(H,L)) & 0x0F) << 4) & 0xFF);
        if(!get_mem(u8_to_u16(H,L))) set_zero();
        break;
    case 0x37://swap nibbles in A
        reset_flags();
        A = (((A & 0xF0) >> 4) & 0x0F) | (((A & 0x0F) << 4 ) & 0xF0);
        if(!A)set_zero();
        break;
    case 0x38://shift b right
        reset_flags();
        if(B & 1)
            set_carry();
        B = (B >> 1) & 0xFF;
        if(!B)
            set_zero();
        break;
    case 0x39://shift C right
        reset_flags();
        if(C & 1)
            set_carry();
        C = (C >> 1) & 0xFF;
        if(!C)
            set_zero();
        break;
    case 0x3A://shift D right
        reset_flags();
        if(D & 1)
            set_carry();
        D = (D >> 1) & 0xFF;
        if(!D)
            set_zero();
        break;
    case 0x3B://shift E right
        reset_flags();
        if(E & 1)
            set_carry();
        E = (E >> 1) & 0xFF;
        if(!E)
            set_zero();
        break;
    case 0x3C://shift H right
        reset_flags();
        if(H & 1)
            set_carry();
        H = (H >> 1) & 0xFF;
        if(!H)
            set_zero();
        break;
    case 0x3D://shift L right
        reset_flags();
        if(L & 1)
            set_carry();
        L = (L >> 1) & 0xFF;
        if(!L)
            set_zero();
        break;
    case 0x3E://shift memory at HL right
        reset_flags();
        tmp_address = u8_to_u16(H,L);
        if(get_mem(tmp_address) & 1)
            set_carry();
        set_mem(tmp_address, (get_mem(tmp_address) >> 1) & 0xFF);
        if(!get_mem(tmp_address))
            set_zero();
        break;
    case 0x3F://shift A right
        reset_flags();
        if(A & 1)
            set_carry();
        A = (A >> 1) & 0xFF;
        if(!A)
            set_zero();
        break;

    case 0x40://Test bit 0 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x41://Test bit 0 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x01))set_zero();
        else unset_zero();
        break; 
    case 0x42://Test bit 0 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x43://Test bit 0 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x44://Test bit 0 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x45://Test bit 0 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x46://Test bit 0 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x01)) set_zero();
        else unset_zero();
        break;
    case 0x47://Test bit 0 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x01))set_zero();
        else unset_zero();
        break;
    case 0x48://Test bit 1 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x49://Test bit 1 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4A://Test bit 1 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4B://Test bit 1 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4C://Test bit 1 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4D://Test bit 1 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4E://Test bit 1 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x02))set_zero();
        else unset_zero();
        break;
    case 0x4F://Test bit 1 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x02))set_zero();
        else unset_zero();
        break;

    case 0x50://Test bit 2 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x51://Test bit 2 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x04))set_zero();
        else unset_zero();
        break; 
    case 0x52://Test bit 2 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x53://Test bit 2 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x54://Test bit 2 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x55://Test bit 2 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x56://Test bit 2 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x57://Test bit 2 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x04))set_zero();
        else unset_zero();
        break;
    case 0x58://Test bit 3 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x59://Test bit 3 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x08))set_zero();
        else unset_zero();
        break; 
    case 0x5A://Test bit 3 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5B://Test bit 3 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5C://Test bit 3 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5D://Test bit 3 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5E://Test bit 3 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x08))set_zero();
        else unset_zero();
        break;
    case 0x5F://Test bit 3 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x08))set_zero();
        else unset_zero();
        break;

    case 0x60://Test bit 4 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x61://Test bit 4 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x10))set_zero();
        else unset_zero();
        break; 
    case 0x62://Test bit 4 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x63://Test bit 4 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x64://Test bit 4 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x65://Test bit 4 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x66://Test bit 4 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x67://Test bit 4 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x10))set_zero();
        else unset_zero();
        break;
    case 0x68://Test bit 5 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x69://Test bit 5 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x20))set_zero();
        else unset_zero();
        break; 
    case 0x6A://Test bit 5 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6B://Test bit 5 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6C://Test bit 5 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6D://Test bit 5 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6E://Test bit 5 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x20))set_zero();
        else unset_zero();
        break;
    case 0x6F://Test bit 5 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x20))set_zero();
        else unset_zero();
        break;

    case 0x70://Test bit 6 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x71://Test bit 6 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x40))set_zero();
        else unset_zero();
        break; 
    case 0x72://Test bit 6 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x73://Test bit 6 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x74://Test bit 6 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x75://Test bit 6 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x76://Test bit 6 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x77://Test bit 6 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x40))set_zero();
        else unset_zero();
        break;
    case 0x78://Test bit 7 of B
        set_halfcarry();
        unset_subtract();
        if(!(B & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x79://Test bit 7 of C
        set_halfcarry();
        unset_subtract();
        if(!(C & 0x80))set_zero();
        else unset_zero();
        break; 
    case 0x7A://Test bit 7 of D
        set_halfcarry();
        unset_subtract();
        if(!(D & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7B://Test bit 7 of E
        set_halfcarry();
        unset_subtract();
        if(!(E & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7C://Test bit 7 of H
        set_halfcarry();
        unset_subtract();
        if(!(H & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7D://Test bit 7 of L
        set_halfcarry();
        unset_subtract();
        if(!(L & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7E://Test bit 7 of value pointed to by HL
        set_halfcarry();
        unset_subtract();
        if(!(get_mem(u8_to_u16(H,L)) & 0x80))set_zero();
        else unset_zero();
        break;
    case 0x7F://Test bit 7 of A
        set_halfcarry();
        unset_subtract();
        if(!(A & 0x80))set_zero();
        else unset_zero();
        break;

    case 0x80://Clear bit 0 of B
        B &= 0xFE;
        break;
    case 0x81://Clear bit 0 of C
        C &= 0xFE;
        break;
    case 0x82://Clear bit 0 of D
        D &= 0xFE;
        break;
    case 0x83://Clear bit 0 of E
        E &= 0xFE;
        break;
    case 0x84://Clear bit 0 of H
        H &= 0xFE;
        break;
    case 0x85://Clear bit 0 of L
        L &= 0xFE;
        break;
    case 0x86://Clear bit 0 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xFE);
        break;
    case 0x87://Clear bit 0 of A
        A &= 0xFE;
        break;
    case 0x88://Clear bit 1 of B
        B &= 0xFD;
        break;
    case 0x89://Clear bit 1 of C
        C &= 0xFD;
        break;
    case 0x8A://Clear bit 1 of D
        D &= 0xFD;
        break;
    case 0x8B://Clear bit 1 of E
        E &= 0xFD;
        break;
    case 0x8C://Clear bit 1 of H
        H &= 0xFD;
        break;
    case 0x8D://Clear bit 1 of L
        L &= 0xFD;
        break;
    case 0x8E://Clear bit 1 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xFD);
        break;
    case 0x8F://Clear bit 1 of A
        A &= 0xFD;
        break;

    case 0x90://Clear bit 2 of B
        B &= 0xFB;
        break;
    case 0x91://Clear bit 2 of C
        C &= 0xFB;
        break;
    case 0x92://Clear bit 2 of D
        D &= 0xFB;
        break;
    case 0x93://Clear bit 2 of E
        E &= 0xFB;
        break;
    case 0x94://Clear bit 2 of H
        H &= 0xFB;
        break;
    case 0x95://Clear bit 2 of L
        L &= 0xFB;
        break;
    case 0x96://Clear bit 2 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xFB);
        break;
    case 0x97://Clear bit 2 of A
        A &= 0xFB;
        break;
    case 0x98://Clear bit 3 of B
        B &= 0xF7;
        break;
    case 0x99://Clear bit 3 of C
        C &= 0xF7;
        break;
    case 0x9A://Clear bit 3 of D
        D &= 0xF7;
        break;
    case 0x9B://Clear bit 3 of E
        E &= 0xF7;
        break;
    case 0x9C://Clear bit 3 of H
        H &= 0xF7;
        break;
    case 0x9D://Clear bit 3 of L
        L &= 0xF7;
        break;
    case 0x9E://Clear bit 3 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xF7);
        break;
    case 0x9F://Clear bit 3 of A
        A &= 0xF7;
        break;

    case 0xA0://Clear bit 4 of B
        B &= 0xEF;
        break;
    case 0xA1://Clear bit 4 of C
        C &= 0xEF;
        break;
    case 0xA2://Clear bit 4 of D
        D &= 0xEF;
        break;
    case 0xA3://Clear bit 4 of E
        E &= 0xEF;
        break;
    case 0xA4://Clear bit 4 of H
        H &= 0xEF;
        break;
    case 0xA5://Clear bit 4 of L
        L &= 0xEF;
        break;
    case 0xA6://Clear bit 4 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xEF);
        break;
    case 0xA7://Clear bit 4 of A
        A &= 0xEF;
        break;
    case 0xA8://Clear bit 5 of B
        B &= 0xDF;
        break;
    case 0xA9://Clear bit 5 of C
        C &= 0xDF;
        break;
    case 0xAA://Clear bit 5 of D
        D &= 0xDF;
        break;
    case 0xAB://Clear bit 5 of E
        E &= 0xDF;
        break;
    case 0xAC://Clear bit 5 of H
        H &= 0xDF;
        break;
    case 0xAD://Clear bit 5 of L
        L &= 0xDF;
        break;
    case 0xAE://Clear bit 5 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xDF);
        break;
    case 0xAF://Clear bit 5 of A
        A &= 0xDF;
        break;

    case 0xB0://Clear bit 6 of B
        B &= 0xBF;
        break;
    case 0xB1://Clear bit 6 of C
        C &= 0xBF;
        break;
    case 0xB2://Clear bit 6 of D
        D &= 0xBF;
        break;
    case 0xB3://Clear bit 6 of E
        E &= 0xBF;
        break;
    case 0xB4://Clear bit 6 of H
        H &= 0xBF;
        break;
    case 0xB5://Clear bit 6 of L
        L &= 0xBF;
        break;
    case 0xB6://Clear bit 6 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0xBF);
        break;
    case 0xB7://Clear bit 6 of A
        A &= 0xBF;
        break;
    case 0xB8://Clear bit 7 of B
        B &= 0x7F;
        break;
    case 0xB9://Clear bit 7 of C
        C &= 0x7F;
        break;
    case 0xBA://Clear bit 7 of D
        D &= 0x7F;
        break;
    case 0xBB://Clear bit 7 of E
        E &= 0x7F;
        break;
    case 0xBC://Clear bit 7 of H
        H &= 0x7F;
        break;
    case 0xBD://Clear bit 7 of L
        L &= 0x7F;
        break;
    case 0xBE://Clear bit 7 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) & 0x7F);
        break;
    case 0xBF://Clear bit 7 of A
        A &= 0x7F;
        break;

    case 0xC0://Set bit 0 of B
      B |= 0x01;
      break;
    case 0xC1://Set bit 0 of C
      C |= 0x01;
      break;
    case 0xC2://Set bit 0 of D
      D |= 0x01;
      break;
    case 0xC3://Set bit 0 of E
      E |= 0x01;
      break;
    case 0xC4://Set bit 0 of H
      H |= 0x01;
      break;
    case 0xC5://Set bit 0 of L
      L |= 0x01;
      break;
    case 0xC6://Set bit 0 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x01);
      break;
    case 0xC7://Set bit 0 of A
      A |= 0x01;
      break;
    case 0xC8://Set bit 1 of B
      B |= 0x02;
      break;
    case 0xC9://Set bit 1 of C
      C |= 0x02;
      break;
    case 0xCA://Set bit 1 of D
      D |= 0x02;
      break;
    case 0xCB://Set bit 1 of E
      E |= 0x02;
      break;
    case 0xCC://Set bit 1 of H
      H |= 0x02;
      break;
    case 0xCD://Set bit 1 of L
      L |= 0x02;
      break;
    case 0xCE://Set bit 1 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x02);
      break;
    case 0xCF://Set bit 1 of A
      A |= 0x02;
      break;

    case 0xD0://Set bit 2 of B
      B |= 0x04;
      break;
    case 0xD1://Set bit 2 of C
      C |= 0x04;
      break;
    case 0xD2://Set bit 2 of D
      D |= 0x04;
      break;
    case 0xD3://Set bit 2 of E
      E |= 0x04;
      break;
    case 0xD4://Set bit 2 of H
      H |= 0x04;
      break;
    case 0xD5://Set bit 2 of L
      L |= 0x04;
      break;
    case 0xD6://Set bit 2 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x04);
      break;
    case 0xD7://Set bit 2 of A
      A |= 0x04;
      break;
    case 0xD8://Set bit 3 of B
      B |= 0x08;
      break;
    case 0xD9://Set bit 3 of C
      C |= 0x08;
      break;
    case 0xDA://Set bit 3 of D
      D |= 0x08;
      break;
    case 0xDB://Set bit 3 of E
      E |= 0x08;
      break;
    case 0xDC://Set bit 3 of H
      H |= 0x08;
      break;
    case 0xDD://Set bit 3 of L
      L |= 0x08;
      break;
    case 0xDE://Set bit 3 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x08);
      break;
    case 0xDF://Set bit 3 of A
      A |= 0x08;
      break;

    case 0xE0://Set bit 4 of B
      B |= 0x10;
      break;
    case 0xE1://Set bit 4 of C
      C |= 0x10;
      break;
    case 0xE2://Set bit 4 of D
      D |= 0x10;
      break;
    case 0xE3://Set bit 4 of E
      E |= 0x10;
      break;
    case 0xE4://Set bit 4 of H
      H |= 0x10;
      break;
    case 0xE5://Set bit 4 of L
      L |= 0x10;
      break;
    case 0xE6://Set bit 4 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x10);
      break;
    case 0xE7://Set bit 4 of A
      A |= 0x10;
      break;
    case 0xE8://Set bit 5 of B
      B |= 0x20;
      break;
    case 0xE9://Set bit 5 of C
      C |= 0x20;
      break;
    case 0xEA://Set bit 5 of D
      D |= 0x20;
      break;
    case 0xEB://Set bit 5 of E
      E |= 0x20;
      break;
    case 0xEC://Set bit 5 of H
      H |= 0x20;
      break;
    case 0xED://Set bit 5 of L
      L |= 0x20;
      break;
    case 0xEE://Set bit 5 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x20);
      break;
    case 0xEF://Set bit 5 of A
      A |= 0x20;
      break;

    case 0xF0://Set bit 6 of B
      B |= 0x40;
      break;
    case 0xF1://Set bit 6 of C
      C |= 0x40;
      break;
    case 0xF2://Set bit 6 of D
      D |= 0x40;
      break;
    case 0xF3://Set bit 6 of E
      E |= 0x40;
      break;
    case 0xF4://Set bit 6 of H
      H |= 0x40;
      break;
    case 0xF5://Set bit 6 of L
      L |= 0x40;
      break;
    case 0xF6://Set bit 6 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x40);
      break;
    case 0xF7://Set bit 6 of A
      A |= 0x40;
      break;
    case 0xF8://Set bit 7 of B
      B |= 0x80;
      break;
    case 0xF9://Set bit 7 of C
      C |= 0x80;
      break;
    case 0xFA://Set bit 7 of D
      D |= 0x80;
      break;
    case 0xFB://Set bit 7 of E
      E |= 0x80;
      break;
    case 0xFC://Set bit 7 of H
      H |= 0x80;
      break;
    case 0xFD://Set bit 7 of L
      L |= 0x80;
      break;
    case 0xFE://Set bit 7 of address at HL
        set_mem(u8_to_u16(H,L),get_mem(u8_to_u16(H,L)) | 0x80);
    break;
    case 0xFF://Set bit 7 of A
      A |= 0x80;
      break;

    default:
        printf("CB opcode 0x%X not implemented yet\n",opcode);
        break;
    }
    cpu_time += cb_table[opcode];
    op_time  += cb_table[opcode];
}

void cpu_writeout_state(u8 opcode){
  if(!memory->in_bios)
    fprintf(files, "opcode %X PC %X SP %X a %X b %X c %X d %X  e %X f %X h %X l %X cycleCounter %lu\n",opcode, PC, SP, A, B, C,D,E,F,H,L, (cpu_time * 4) - 23676308);
}

void cpu_step(u8 opcode){
    u16 tmp;
    int signed_tmp;
    op_time = 0;
    cpu_writeout_state(opcode);
    switch(opcode){
    case 0x00://no-op
        break;
    case 0x01://load 16bit immediate into BC
        C=get_mem(PC++);
        B=get_mem(PC++);
        break;
    case 0x02://Save A to address pointed by BC
        set_mem(u8_to_u16(B,C),A);
        break;
    case 0x03: // INC BC
        tmp = inc_16(u8_to_u16(B,C));
        B = (tmp >> 8) & 0xFF;
        C = tmp & 0xFF;
        break;
    case 0x04://INC B
        B = inc_8(B);
        break;
    case 0x05://Dec B
        B = dec_8(B);
        break;
    case 0x06://Load immediate into B
        B = get_mem(PC++);
        break;
    case 0x07:// rotate left through carry accumulator
        A = rot_left_carry_8(A);
        break;
    case 0x08://save sp to a given address
        tmp = (get_mem(PC+1) << 8) + get_mem(PC);
        set_mem(tmp, (SP & 0xFF));
        set_mem(tmp+1, (SP >> 8) & 0xFF);
        PC+=2;
        break;
    case 0x09://Add BC to HL
        tmp = add_16(u8_to_u16(B,C),u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x0A://Load A from Addres pointed to by BC
        A = get_mem(u8_to_u16(B,C));
        break;
    case 0x0B://Dec BC
        tmp = dec_16(u8_to_u16(B,C));
        B = (tmp >> 8) & 0xFF;
        C = tmp & 0xFF;
        break;
    case 0x0C://INC C
        C = inc_8(C);
        break;
    case 0x0D://Dec C
        C = dec_8(C);
        break;
    case 0x0E://load 8-bit immediate into C
        C = get_mem(PC++);
        break;
    case 0x0F://rotate right carry accumulator
        A = rot_right_carry_8(A);
        break;

    case 0x10://Stop cpu and lcd until a button is pressed
        printf("Processor stopping\n");
        cpu_stop = 1;
        break;
    case 0x11://load 16bit immediate into DE
        E = get_mem(PC++);
        D = get_mem(PC++);
        break;
    case 0x12://Save A to address pointed by DE
        set_mem(u8_to_u16(D,E),A);
        break;
    case 0x13: //INC DE
        tmp = inc_16(u8_to_u16(D,E));
        D = (tmp >> 8) & 0xFF;
        E = tmp & 0xFF;
        break;
    case 0x14://INC D
        D = inc_8(D);
        break;
    case 0x15://Dec D
        D = dec_8(D);
        break;
    case 0x16://Load immediate into D
        D = get_mem(PC++);
        break;
    case 0x17://rotate accumulator left
        A = rot_left_8(A);
        break;
    case 0x18://Relative jump by signed immediate
        PC += (signed char)get_mem(PC);
        PC++;//to jump over the immediate value
        break;
    case 0x19://Add DE to HL
        tmp = add_16(u8_to_u16(D,E),u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x1A://Load A from Addres pointed to by DE
        A = get_mem(u8_to_u16(D,E));
        break;
    case 0x1B://Dec DE
        tmp = dec_16(u8_to_u16(D,E));
        D = (tmp >> 8) & 0xFF;
        E = tmp & 0xFF;
        break;
    case 0x1C://INC E
        E = inc_8(E);
        break;
    case 0x1D://Dec E
        E = dec_8(E);
        break;
    case 0x1E://load 8-bit immediate into E
        E = get_mem(PC++);
        break;
    case 0x1F://rotate accumulator right
        A = rot_right_8(A);
        break;

    case 0x20://Relative jump by signed immediate if last result was not zero
        signed_tmp = (signed char)get_mem(PC);
        if(!(F & 0x80)){
            PC += signed_tmp;
            cpu_time += 1;
            op_time += 1;
        }
        PC++;
        break;
    case 0x21://load 16bit immediate into HL
        L = get_mem(PC++);
        H = get_mem(PC++);
        break;
    case 0x22://Save A to address pointed by HL and increment HL
        set_mem(u8_to_u16(H,L),A);
        tmp = inc_16(u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x23: //INC HL
        tmp = inc_16(u8_to_u16(H,L));
        H = ((tmp & 0xFF00) >> 8);
        L = tmp & 0xFF;
        break;
    case 0x24://INC H
        H = inc_8(H);
        break;
    case 0x25://Dec H
        H = dec_8(H);
        break;
    case 0x26://Load immediate into H
        H = get_mem(PC++);
        break;
    case 0x27:// DAA
    {
        int a = A;
        if(!(F & 0x40)){
            if((F & 0x20) || (a & 0x0F) > 9)
                a += 0x06;
            if((F & 0x10) || (a > 0x9F))
                a += 0x60;
        }
        else{
            if(F & 0x20)
                a = (a - 6) & 0xFF;
            if(F & 0x10)
                a -= 0x60;
        }
        unset_halfcarry();

        if((a & 0x100))
            set_carry();

        a &= 0xFF;

        if(a == 0)
            set_zero();
        else
            unset_zero();
        A = (u8)a;
    }
        break;
    case 0x28://Relative jump by signed immediate if last result caused a zero
        signed_tmp = (signed char)get_mem(PC);
        if(F & 0x80){
            PC += signed_tmp;
            cpu_time += 1;
            op_time += 1;
        }
        PC++;
        break;
    case 0x29://Add HL to HL
        tmp = add_16(u8_to_u16(H,L),u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x2A://Load A from Address pointed to by HL and INC HL
        A = get_mem(u8_to_u16(H,L));
        tmp = inc_16(u8_to_u16(H,L));
        H = (tmp >> 8);
        L = tmp & 0xFF;
        break;
    case 0x2B://Dec HL
        tmp = dec_16(u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x2C://INC L
        L = inc_8(L);
        break;
    case 0x2D://Dec L
        L = dec_8(L);
        break;
    case 0x2E://load 8-bit immediate into L
        L = get_mem(PC++);
        break;
    case 0x2F://NOT A/Complement of A
        A = ~A;
        set_subtract();
        set_halfcarry();
        break;
    
    case 0x30://Relative jump by signed immediate if last result was not carry
        signed_tmp = (signed char)get_mem(PC);
        if(!(F & 0x10)){
            PC+=signed_tmp;
            cpu_time += 1;
            op_time += 1;
        }
        PC++;
        break;
    case 0x31://load 16bit immediate into SP
        SP = (get_mem(PC+1) << 8) + get_mem(PC);
        PC += 2;
        break;
    case 0x32://Save A to address pointed by HL and dec HL
        set_mem(u8_to_u16(H,L),A);
        tmp = dec_16(u8_to_u16(H,L));
        H= (tmp >> 8) & 0xFF;
        L = (tmp & 0xFF);
        break;
    case 0x33: //INC SP
        SP = inc_16(SP);
        break;
    case 0x34://INC (HL)
        set_mem(u8_to_u16(H,L), inc_8(get_mem(u8_to_u16(H,L))));
        break;
    case 0x35://DEC (HL)
        set_mem(u8_to_u16(H,L), dec_8(get_mem(u8_to_u16(H,L))));
        break;
    case 0x36://Load immediate into address pointed by HL
        set_mem(u8_to_u16(H,L),get_mem(PC++));
        break;
    case 0x37://set carry flag
        unset_subtract();
        unset_halfcarry();
        set_carry();
        break;
    case 0x38://Relative jump by signed immediate if last result caused a carry
        signed_tmp = (signed char)get_mem(PC);
        if(F & 0x10){
            PC += signed_tmp;
            cpu_time += 1;
            op_time += 1;
        }
        PC++;
        break;
    case 0x39://Add SP to HL
        tmp = add_16(SP,u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x3A://Load A from Addres pointed to by HL and DEC HL
        A = get_mem(u8_to_u16(H,L));
        tmp = dec_16(u8_to_u16(H,L));
        H = (tmp >> 8) & 0xFF;
        L = tmp & 0xFF;
        break;
    case 0x3B://Dec SP
        SP = dec_16(SP);
        break;
    case 0x3C://Inc A
        A = inc_8(A);
        break;
    case 0x3D://Dec A
        A = dec_8(A);
        break;
    case 0x3E://load 8-bit immediate into A
        A = get_mem(PC++);
        break;
    case 0x3F://Complement carry flag
        unset_subtract();
        unset_halfcarry();
        if(F & 0x10)
            unset_carry();
        else
            set_carry();
        break;

    case 0x40://Copy B to B
        break;
    case 0x41://Copy C to B
        B = C;
        break;
    case 0x42://Copy D to B
        B = D;
        break;
    case 0x43://Copy E to B
        B = E;
        break;
    case 0x44://Copy H to B
        B = H;
        break;
    case 0x45://Copy L to B
        B = L;
        break;
    case 0x46://Copy value pointed by HL to B
        B = get_mem(u8_to_u16(H,L));
        break;
    case 0x47://Copy A to B
        B = A;
        break;
    case 0x48://Copy B to C
        C = B;
        break;
    case 0x49://Copy C to C
        break;
    case 0x4A://Copy D to C
        C = D;
        break;
    case 0x4B://Copy E to C
        C = E;
        break;
    case 0x4C://Copy H to C
        C = H;
        break;
    case 0x4D://Copy L to C
        C = L;
        break;
    case 0x4E://Copy value pointed by HL to C
        C = get_mem(u8_to_u16(H,L));
        break;
    case 0x4F://Copy A to C
        C = A;
        break;

    case 0x50://Copy B to D
        D = B;
        break;
    case 0x51://Copy C to D
        D = C;
        break;
    case 0x52://Copy D to D
        break;
    case 0x53://Copy E to D
        D = E;
        break;
    case 0x54://Copy H to D
        D = H;
        break;
    case 0x55://Copy L to D
        D = L;
        break;
    case 0x56://Copy value pointed by HL to D
        D = get_mem(u8_to_u16(H,L));
        break;
    case 0x57://Copy A to D
        D = A;
        break;
    case 0x58://Copy B to E
        E = B;
        break;
    case 0x59://Copy C to E
        E = C;
        break;
    case 0x5A://Copy D to E
        E = D;
        break;
    case 0x5B://Copy E to E
        break;
    case 0x5C://Copy H to E
        E = H;
        break;
    case 0x5D://Copy L to E
        E = L;
        break;
    case 0x5E://Copy value pointed by HL to E
        E = get_mem(u8_to_u16(H,L));
        break;
    case 0x5F://Copy A to E
        E = A;
        break;

    case 0x60://Copy B to H
        H = B;
        break;
    case 0x61://Copy C to H
        H = C;
        break;
    case 0x62://Copy D to H
        H = D;
        break;
    case 0x63://Copy E to H
        H = E;
        break;
    case 0x64://Copy H to H
        break;
    case 0x65://Copy L to H
        H = L;
        break;
    case 0x66://Copy value pointed by HL to H
        H = get_mem(u8_to_u16(H,L));
        break;
    case 0x67://Copy A to H
        H = A;
        break;
    case 0x68://Copy B to L
        L = B;
        break;
    case 0x69://Copy C to L
        L = C;
        break;
    case 0x6A://Copy D to L
        L = D;
        break;
    case 0x6B://Copy E to L
        L = E;
        break;
    case 0x6C://Copy H to L
        L = H;
        break;
    case 0x6D://Copy L to L
        break;
    case 0x6E://Copy value pointed by HL to L
        L = get_mem(u8_to_u16(H,L));
        break;
    case 0x6F://Copy A to L
        L = A;
        break;

    case 0x70://copy B to Address pointed to by HL
        set_mem(u8_to_u16(H,L),B);
        break;
    case 0x71://copy C to Address pointed to by HL
        set_mem(u8_to_u16(H,L),C);
        break;
    case 0x72://copy D to Address pointed to by HL
        set_mem(u8_to_u16(H,L),D);
        break;
    case 0x73://copy E to Address pointed to by HL
        set_mem(u8_to_u16(H,L),E);
        break;
    case 0x74://copy H to Address pointed to by HL
        set_mem(u8_to_u16(H,L),H);
        break;
    case 0x75://copy L to Address pointed to by HL
        set_mem(u8_to_u16(H,L),L);
        break;
    case 0x76://HALT
        printf("halting cpu\n");
        cpu_halt = 1;
        break;
    case 0x77://copy A to Address pointed to by HL
        set_mem(u8_to_u16(H,L),A);
        break;
    case 0x78:// copy B to A
        A=B;
        break;
    case 0x79:// copy C to A
        A=C;
        break;
    case 0x7A:// copy D to A
        A=D;
        break;
    case 0x7B:// copy E to A
        A=E;
        break;
    case 0x7C:// copy H to A
        A=H;
        break;
    case 0x7D:// copy L to A
        A=L;
        break;
    case 0x7E://Copy value pointed by HL to A
        A = get_mem(u8_to_u16(H,L));
        break;
    case 0x7F:// copy A to A
        break;

    case 0x80://ADD B to A
        A = add_8(B,A);
        break;
    case 0x81://ADD C to A
        A = add_8(C,A);
        break;
    case 0x82://ADD D to A
        A = add_8(D,A);
        break;
    case 0x83://ADD E to A
        A = add_8(E,A);
        break;
    case 0x84://ADD H to A
        A = add_8(H,A);
        break;
    case 0x85://ADD L to A
        A = add_8(L,A);
        break;
    case 0x86://ADD value pointed by HL to A
        A = add_8(get_mem(u8_to_u16(H,L)),A);
        break;
    case 0x87://ADD A to A
        A = add_8(A,A);
        break;
    case 0x88://Add B and carry flag to A
        A = add_8c(A,B);
        break;
    case 0x89://Add C and carry flag to A
        A = add_8c(A,C);
        break;
    case 0x8A://Add D and carry flag to A
        A = add_8c(A,D);
        break;
    case 0x8B://Add E and carry flag to A
        A = add_8c(A,E);
        break;
    case 0x8C://Add H and carry flag to A
        A = add_8c(A,H);
        break;
    case 0x8D://Add L and carry flag to A
        A = add_8c(A,L);
        break;
    case 0x8E://Add value pointed by HL and carry flag to A
        A = add_8c(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0x8F://Add A and carry flag to A
        A = add_8c(A,A);
        break;

    case 0x90://SUB B from A
        A = sub_8(A,B);
        break;
    case 0x91://SUB C from A
        A = sub_8(A,C);
        break;
    case 0x92://SUB D from A
        A = sub_8(A,D);
        break;
    case 0x93://SUB E from A
        A = sub_8(A,E);
        break;
    case 0x94://SUB H from A
        A = sub_8(A,H);
        break;
    case 0x95://SUB L from A
        A = sub_8(A,L);
        break;
    case 0x96://SUB value pointed by HL from A
        A = sub_8(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0x97://SUB A from A
        A = sub_8(A,A);
        break;
    case 0x98://SUB B and carry flag from A
        A = sub_8c(A,B);
        break;
    case 0x99://SUB C and carry flag from A
        A = sub_8c(A,C);
        break;
    case 0x9A://SUB D and carry flag from A
        A = sub_8c(A,D);
        break;
    case 0x9B://SUB E and carry flag from A
        A = sub_8c(A,E);
        break;
    case 0x9C://SUB H and carry flag from A
        A = sub_8c(A,H);
        break;
    case 0x9D://SUB L and carry flag from A
        A = sub_8c(A,L);
        break;
    case 0x9E://SUB value pointed by HL and carry flag from A
        A = sub_8c(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0x9F://SUB A and carry flag from A
        A = sub_8c(A,A);
        break;
    case 0xA0://AND A with B
        A = and_8(A,B);
        break;
    case 0xA1://AND A with C
        A = and_8(A,C);
        break;
    case 0xA2://AND A with D
        A = and_8(A,D);
        break;
    case 0xA3://AND A with E
        A = and_8(A,E);
        break;
    case 0xA4://AND A with H
        A = and_8(A,H);
        break;
    case 0xA5://AND A with L
        A = and_8(A,L);
        break;
    case 0xA6://AND A with value pointed to by HL
        A = and_8(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0xA7://And A with A
        A = and_8(A,A);
        break;
    case 0xA8://XOR B with A
        A = xor_8(A,B);
        break;
    case 0xA9://XOR C with A
        A = xor_8(A,C);
        break;
    case 0xAA://XOR D with A
        A = xor_8(A,D);
        break;
    case 0xAB://XOR E with A
        A = xor_8(A,E);
        break;
    case 0xAC://XOR H with A
        A = xor_8(A,H);
        break;
    case 0xAD://XOR L with A
        A = xor_8(A,L);
        break;
    case 0xAE://XOR A with value pointed to by HL
        A = xor_8(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0xAF://XOR A with A
        A = xor_8(A,A);
        break;

    case 0xB0://OR A with B
        A = or_8(A,B);
        break;
    case 0xB1://OR A with C
        A = or_8(A,C);
        break;
    case 0xB2://OR A with D
        A = or_8(A,D);
        break;
    case 0xB3://OR A with E
        A = or_8(A,E);
        break;
    case 0xB4://OR A with H
        A = or_8(A,H);
        break;
    case 0xB5://OR A with L
        A = or_8(A,L);
        break;
    case 0xB6://OR A with value pointed by HL
        A = or_8(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0xB7://OR A with A
        A = or_8(A,A);
        break;
    case 0xB8://compare B against A
        comp_8(A,B);
        break;
    case 0xB9://compare C against A
        comp_8(A,C);
        break;
    case 0xBA://compare D against A
        comp_8(A,D);
        break;
    case 0xBB://compare E against A
        comp_8(A,E);
        break;
    case 0xBC://compare H against A
        comp_8(A,H);
        break;
    case 0xBD://compare L against A
        comp_8(A,L);
        break;
    case 0xBE://compare value pointed by HL against A
        comp_8(A,get_mem(u8_to_u16(H,L)));
        break;
    case 0xBF://compare A against A
        comp_8(A,A);
        break;

    case 0xC0://Return if last result was not zero
        if(!(F & 0x80)){
            PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
            SP +=2;
            cpu_time += 3;
            op_time += 3;
        }
        break;
    case 0xC1://POP stack into BC
        C = get_mem(SP++);
        B = get_mem(SP++);
        break;
    case 0xC2://Absolute jump to 16 bit location if last result not zero
        if(!(F & 0x80)){
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 1;
            op_time += 1;
        }else{
            PC+=2;
        }
        break;
    case 0xC3://Absolute jump to 16 bit location
        PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
        break;
    case 0xC4://call routine at 16 bit immediate if last result not zero
        if(!(F& 0x80)){
            set_mem(--SP,((PC+2) >> 8) & 0xFF);
            set_mem(--SP,(PC+2) & 0xFF);
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 3;
            op_time += 3;
        }else{
            PC+=2;
        }
        break;
    case 0xC5://PUSH BC onto stack
        set_mem(--SP,B);
        set_mem(--SP,C);
        break;
    case 0xC6://ADD immediate to A
        A = add_8(A,get_mem(PC++));
        break;
    case 0xC7://call routine at 0
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,PC & 0xFF);
        PC = 0x00;
        break;
    case 0xC8://Return if last result was zero
        if(F&0x80){
            PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
            SP +=2;
            cpu_time += 3;
            op_time += 3;
        }
        break;
    case 0xC9://Return
        PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
        SP += 2;
        break;
    case 0xCA://Absolute jump to 16 bit location if last result zero
        if(F & 0x80){
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 1;
            op_time += 1;
        }else{
            PC+=2;
        }
        break;
    case 0xCB://double byte instruction
        cb_opcodes(get_mem(PC++));
        break;
    case 0xCC://call routine at 16 bit immediate if last result was zero
        if(F& 0x80){
            set_mem(--SP,((PC+2) >> 8) & 0xFF);
            set_mem(--SP,(PC+2) & 0xFF);
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 3;
            op_time += 3;
        }else{
            PC+=2;
        }
        break;
    case 0xCD://call routine at 16 bit immediate
        set_mem(--SP,((PC+2) >> 8) & 0xFF);
        set_mem(--SP, (PC+2) & 0xFF);
        PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
        break;
    case 0xCE://ADD immediate and carry to A
        A = add_8c(A,get_mem(PC++));
        break;
    case 0xCF://call routine at 0x0008
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP, PC & 0xFF);
        PC = 0x08;
        break;
    case 0xD0://Return if last result was not carry
        if(!(F&0x10)){
            PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
            SP +=2;
            cpu_time += 3;
            op_time += 3;
        }
        break;
    case 0xD1://POP stack into DE
        E = get_mem(SP++);
        D = get_mem(SP++);
        break;
    case 0xD2://Absolute jump to 16 bit location if last result not carry
        if(!(F & 0x10)){
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 1;
            op_time += 1;
        }else{
            PC+=2;
        }
        break;
    case 0xD4://call routine at 16 bit immediate if last result not carry
        if(!(F& 0x10)){
            //SP-=2;
            set_mem(--SP,((PC + 2) >> 8) & 0xFF);
            set_mem(--SP,((PC + 2) & 0xFF) );

            //set_mem_16(SP,PC+2);//save program counter to stack
            PC = u8_to_u16(get_mem(PC + 1), get_mem(PC));
            cpu_time += 3;
            op_time += 3;
        }else{
            PC+=2;
        }
        break;
    case 0xD5://PUSH DE onto stack
        set_mem(--SP,D);
        set_mem(--SP,E);
        break;
    case 0xD6://SUB immediate against A
        A = sub_8(A,get_mem(PC++));
        break;
    case 0xD7://call routine at 0x10
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x10;
        break;
    case 0xD8://Return if last result was carry
        if(F&0x10){
            PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
            SP += 2;
            cpu_time += 3;
            op_time += 3;
        }
        break;
    case 0xD9://enable interrupts and return
        interrupts = 1;
        load_regs();
        PC = u8_to_u16(get_mem(SP+1),(get_mem(SP)));
        SP += 2;
        break;
    case 0xDA://Absolute jump to 16 bit immediate if last result carry
        if(F & 0x10){
            PC = u8_to_u16(get_mem(PC+1),get_mem(PC));
            cpu_time += 1;
            op_time += 1;
        }else{
            PC+=2;
        }
        break;
    case 0xDB:// not used
        break;
    case 0xDC://call routine at 16 bit immediate if last result carry
        if(F & 0x10){
            set_mem(--SP,((PC + 2) >> 8) & 0xFF);
            set_mem(--SP,((PC + 2) & 0xFF) );
            PC = u8_to_u16(get_mem(PC + 1),get_mem(PC));
            cpu_time += 3;
            op_time += 3;
        }else{
            PC+=2;
        }
        break;
    case 0xDD://not used
        break;
    case 0xDE://Subtract immediate and carry from A
        A = sub_8c(A,get_mem(PC++));
        break;
    case 0xDF://call routine at 0x0018
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x18;
        break;
    
    case 0xE0://save A at address pointed to by 0xFF00 + immediate
        set_mem(0xFF00 + get_mem(PC++),A);
        break;
    case 0xE1://POP stack into HL
        L = get_mem(SP++);
        H = get_mem(SP++);
        break;
    case 0xE2://save A at address pointed to by 0xFF00 + C
        set_mem(0xFF00 + C,A);
        break;
    case 0xE3://not used
        break;
    case 0xE4://not used
        break;
    case 0xE5://PUSH HL onto stack
        set_mem(--SP,H);
        set_mem(--SP,L);
        break;
    case 0xE6://AND immediate against A
        A = and_8(A,get_mem(PC++));
        break;
    case 0xE7://call routine at 0x20
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x20;
        break;
    case 0xE8://add signed 8bit immediate to SP
    {
      int number = (signed char)get_mem(PC++);
        int result = SP + number;
        reset_flags();
        if((SP ^ number ^ (result & 0xFFFF)) & 0x100)
           set_carry();
        if((SP ^ number ^ (result & 0xFFFF)) & 0x10)
            set_halfcarry();
        SP = (u16)(result);
    }
    break;
    case 0xE9://PC equals HL
        PC = u8_to_u16(H,L);
        break;
    case 0xEA://save A at 16bit immediate given address
        set_mem(u8_to_u16(get_mem(PC+1),get_mem(PC)),A);
        PC+=2;
        break;
    case 0xEB://not used
        break;
    case 0xEC://not used
        break;
    case 0xED://not used
        break;
    case 0xEE://xor 8 bit immediate against A
        A = xor_8(A,get_mem(PC++));
        break;
    case 0xEF://call routine at 0x28
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x28;
        break;

    case 0xF0://load A from address pointed to by 0xFF00 + immediate 8bit
        A = get_mem(0xFF00 + get_mem(PC++));
        break;
    case 0xF1://POP stack into AF low nibbles of flag register should be zeroed
        F = get_mem(SP++) & 0xF0;
        A = get_mem(SP++);
        break;
    case 0xF2://Put value at address $FF00 + register C into A
        A = get_mem(0xFF00 + C);
        break;
    case 0xF3://Disable interrupts. Interrupts are disabled after instruction after this one is executed
        interrupts = 0;
        break;
    case 0xF5://PUSH AF onto stack
        set_mem(--SP,A);
        set_mem(--SP,F);
        break;
    case 0xF6://OR immediate against A
        A = or_8(A,get_mem(PC++));
        break;
    case 0xF7://call routine at 0x30
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x30;
        break;
    case 0xF8://Add signed immediate to SP and save result in HL
    {
      int number = (signed char)get_mem(PC++);
        int result = SP + number;
        reset_flags();
        if((SP ^ number ^ (result & 0xFFFF)) & 0x100)
           set_carry();
        if((SP ^ number ^ (result & 0xFFFF)) & 0x10)
            set_halfcarry();
        H = (result >> 8) & 0xFF;
        L = result & 0xFF;
    }    
    break;
    case 0xF9://copy HL to SP
        SP = u8_to_u16(H,L);
        break;
    case 0xFA://load A from given address
        A = get_mem(u8_to_u16(get_mem(PC+1),get_mem(PC)));
        PC += 2;
        break;
    case 0xFB://Enable interrupts. Interrupts are enabled after instruction after this one is executed
        interrupts = 1;
        break;
    case 0xFC://not used
        break;
    case 0xFD://not used
        break;
    case 0xFE://compare 8 bit immediate against A
        comp_8(A, get_mem(PC++));
        break;
    case 0xFF://call routine at 0x38
        set_mem(--SP,(PC >> 8) & 0xFF);
        set_mem(--SP,(PC & 0xFF) );
        PC = 0x38;
        break;

    default:
        printf("%X Not implemented yet\n",opcode);
        break;
    }
    cpu_time += t[opcode];
    op_time += t[opcode];
}



static void vblank_interrupt(){//RST-40
    store_regs();
    interrupts = 0;
    set_mem(--SP,(PC >> 8) & 0xFF);
    set_mem(--SP,(PC & 0xFF) );
    PC = 0x0040;
    op_time += 3;
    cpu_time += 3;
}      

void print_cpu(){
    printf("A: %X F: %X B: %X C: %X D: %X E: %X H: %X L: %X\n",A,F,B,C,D,E,H,L);
    printf("SP: %X\n",SP);
    printf("PC:%X\n",PC);
    printf("Time %lu cycles\n",cpu_time);
}

void cpu_init(){
    files = fopen("out","w");
    if(!files){
        printf("couldn't open out");
        exit(1);
    }
    A = B = C = D = E = H = L = F = 0;
    PC = 0;
    SP = 0;
    interrupts = 0;
    cpu_time = 0;
    cpu_halt=0;
    op_time=0;
    cpu_exit_loop=0;
}

void cpu_exit(){
    cpu_exit_loop = 1;
}

void cpu_run_once(){
    print_cpu();
    cpu_step(get_mem(PC++));

    printf("opcode: %X\n",get_mem(PC-1));
    gpu_step(op_time);
    PC &= 0xFFFF;
    print_cpu();
}

void cpu_run(){
    while(!cpu_exit_loop){
        cpu_step(get_mem(PC++));
        // should check that the last instruction wasn't halt
        if(interrupts && memory->interrupt_enable && memory->interrupt_flags){
            int fired = memory->interrupt_enable & memory->interrupt_flags;
            if(fired & 1){
                memory->interrupt_flags &= (0xFE);
                vblank_interrupt();
            }
        }
        gpu_step(op_time);
        PC &= 0xFFFF;
    }
}
