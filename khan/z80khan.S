/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hardware/regs/sio.h"
.syntax unified
.cpu cortex-m0plus
.thumb

#define _u(x) x
#define INTERP_OFFSET0(x) ((x) - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP_OFFSET1(x) (INTERP_OFFSET0(x) + SIO_INTERP1_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)

#undef ENABLE_BREAKPOINT_IN_DEBUG

#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
#define ENABLE_BREAKPOINT
#endif
//#define ENABLE_ACTIVE_ASSERTION
//#define ENABLE_64K_ADDRESS_CHECK
#endif

// moved everything to data for now (ROM 32K limit), but we need it to be fast anyway
.section .data_z80a,"x"
#define CF 0x01
#define NF 0x02
#define PV 0x04
#define F3 0x08
#define HF 0x10
#define F5 0x20
#define ZF 0x40
#define SF 0x80

#define CF_INDEX 0
#define NF_INDEX 1
#define PV_INDEX 2
#define F3_INDEX 3
#define HF_INDEX 4
#define F5_INDEX 5
#define ZF_INDEX 6
#define SF_INDEX 7

	/**
	 * r0 -
	 * r1
	 * r2 - temp_restricted (a scratch register)
	 * r3 - temp (either a temporary 16 bit register for intra instruction work, or the current ix/iy for xy functions (none of which need a temp register))
	 * r4 - pc
	 * r5 - memory (either a single 64k block, or an array of 8 pointers to 16K blocks; 4 for read, 4 for write) dependeing on mode
	 * r6 - t
	 * r7 - af
	 * r8 - bc
	 * r9 - de
	 * r10 - hl
	 * r11 - memptr
	 * r12 - sp
	 */

#define r_temp_restricted r2
#define r_temp r3
#define r_ixy r3

#define r_pc r4
#ifndef USE_BANKED_MEMORY_ACCESS
#define memory r5
#else
#define interp_base r5
#endif
#define r_t r6
#define r_af r7
#define r_bc r8
#define r_de r9
#define r_hl r10
#define r_memptr r11
#define r_sp r12

.macro preserve_only_flags reg flags:vararg
    movs \reg, #255 ^ (\flags)
    bics r_af, \reg
.endm

.macro read8_internal addr_reg
#ifdef ENABLE_64K_ADDRESS_CHECK
.ifc r0,\addr_reg
    push {r0}
.endif
    lsrs r0, \addr_reg, #16
.ifc r0,\addr_reg
    pop {r0}
.endif
    beq 8f
    bkpt #0
8:
#endif
#ifndef USE_BANKED_MEMORY_ACCESS
    ldrb r0, [memory, \addr_reg]
#else
    str \addr_reg, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    ldrb r0, [r1, \addr_reg]
#endif
.endm

.macro write8_internal_r1
#ifdef ENABLE_64K_ADDRESS_CHECK
    lsrs r2, r1, #16
    beq 8f
    bkpt #0
8:
#endif
#ifndef USE_BANKED_MEMORY_ACCESS
    lsrs r2, r1, #14
    beq 1f
    strb r0, [memory, r1]
1:
#else
    // if we can spare 16k we can use it as a dumpground for writes to ROM
    str r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    strb r0, [r2, r1]
#endif
.endm

// addr_reg may not be r0
// seems like r2 is spare
.macro read8inc_internal addr_reg
#ifdef ENABLE_64K_ADDRESS_CHECK
    lsrs r0, \addr_reg, #16
    beq 8f
    bkpt #0
8:
#endif
#ifndef USE_BANKED_MEMORY_ACCESS
    ldrb r0, [memory, \addr_reg]
#else
    // todo one offset for read, one for write... we can do a dump bank
    str \addr_reg, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r0, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r0, [r0]
    ldrb r0, [r0, \addr_reg]
#endif
    adds \addr_reg, #1
    uxth \addr_reg, \addr_reg
.endm

.macro read16_internal_r0
#ifdef ENABLE_64K_ADDRESS_CHECK
    lsrs r1, r0, #16
    beq 8f
    bkpt #0
8:
#endif
    lsrs r1, r0, #1
    bcs 1f // unaligned
#ifndef USE_BANKED_MEMORY_ACCESS
    ldrh r0, [memory, r0]
    b 2f
1:
    ldrb r1, [memory, r0]
    adds  r0, #1
    uxth r0, r0
    ldrb r0, [memory, r0]
    lsls r0, #8
    orrs r0, r1
#else
    str r0, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    ldrh r0, [r1, r0]
    b 2f
1:
    str r0, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    ldrb r1, [r1, r0]
    adds  r0, #1
    str r0, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r2, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    uxth r0, r0 ; // we need to wrap the address since we use it as an offset
    ldr r2, [r2]
    ldrb r0, [r2, r0]
    lsls r0, #8
    orrs r0, r1
#endif
2:
.endm

.macro read16inc_internal_r1
#ifdef ENABLE_64K_ADDRESS_CHECK
    lsrs r0, r1, #16
    beq 8f
    bkpt #0
8:
#endif
    lsrs r0, r1, #1
    bcs 1f // unaligned
#ifndef USE_BANKED_MEMORY_ACCESS
    ldrh r0, [memory, r1]
    adds r1, #2
    b 2f
1:
    ldrb r0, [memory, r1]
    adds r1, #1
    uxth r1, r1
    ldrb r2, [memory, r1]
    lsls r2, #8
    orrs r0, r2
    adds r1, #1
#else
    str r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r0, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r0, [r0]
    ldrh r0, [r0, r1]
    adds r1, #2
    b 2f
1:
    str r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r2, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    ldrb r0, [r2, r1]
    adds  r1, #1
    str r1, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    uxth r1, r1
    ldr r2, [interp_base, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    ldrb r2, [r2, r1]
    lsls  r2, #8
    orrs  r0, r2
    adds r1, #1
#endif
2:
    uxth r1, r1
.endm

.macro write16_internal_r1
#ifdef ENABLE_64K_ADDRESS_CHECK
    lsrs r2, r1, #16
    beq 8f
    bkpt #0
8:
#endif
    lsrs r2, r1, #1
    bcs 1f // unaligned
#ifndef USE_BANKED_MEMORY_ACCESS
    lsrs r2, r1, #14
    beq 3f
    strh r0, [memory, r1]
    b 3f
1:
    lsrs r2, r1, #14
    beq 2f // skip first write
    strb r0, [memory, r1]
2:
    adds  r1, #1
    uxth r1, r1
    lsrs r2, r1, #14
    beq 3f // skip second write
    lsrs r0, #8
    strb r0, [memory, r1]
3:
#else
    str r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    strh r0, [r2, r1]
    b 3f
1:
    str r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    strb r0, [r2, r1]
2:
    adds r1, #1
    str r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    uxth r1, r1
    ldr r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r2, [r2]
    lsrs r0, #8
    strb r0, [r2, r1]
3:
#endif
.endm

.macro fetch
#ifndef NO_USE_REPLAY
#error Need: --fetches;
#endif
#ifndef NO_UPDATE_RLOW_IN_FETCH
	ldr  r2, =z80a_resting_state
	ldrb r0, [r2, #(r_low - z80a_resting_state)]
	adds  r0, #1
	strb r0, [r2, #(r_low - z80a_resting_state)]
#endif
    adds r_t, #4
    read8inc_internal r_pc
.endm

.global z80a_reset
.type z80a_reset,%function
.thumb_func
z80a_reset:
    movs r0, #0
    adr r1, z80a_resting_state
    str r0, [r1, #(_int_flags - z80a_resting_state)]
    str r0, [r1, #(ir_register - z80a_resting_state)]
    str r0, [r1, #(_im - z80a_resting_state)]
    str r0, [r1, #(saved_r_pc - z80a_resting_state)]
    bx lr

z80a_load_state:
    adr r0, saved_register_file_hi
#ifdef ENABLE_ACTIVE_ASSERTION
    ldr r1, [r0, #(is_active - saved_register_file_hi)]
    cmp r1, #0
    beq 1f
    bkpt #0 // assertion fail
1:
#endif
    movs r1, #1
    str r1, [r0, #(is_active - saved_register_file_hi)]
    ldm r0!, {r1, r2, r3, r4, r5}
    mov r_bc, r1
    mov r_de, r2
    mov r_hl, r3
    mov r_memptr, r4
    mov r_sp, r5
    adr r0, saved_register_file_lo
    ldm r0!, { r_pc, r5, r_t, r_af}
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r2, frame_tacts
    subs r_t, r2
#endif
    bx  lr

z80a_unload_state:
    adr r0, saved_register_file_lo
#ifdef ENABLE_ACTIVE_ASSERTION
    ldr r1, [r0, #(is_active - saved_register_file_lo)]
    cmp r1, #1
    beq 1f
    bkpt #0 // assertion fail
1:
#endif
    movs r1, #0
    str r1, [r0, #(is_active - saved_register_file_lo)]
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r2, frame_tacts
    adds r_t, r2
#endif
    stm r0!, { r_pc, r5, r_t, r_af}
    mov r1, r_bc
    mov r2, r_de
    mov r3, r_hl
    mov r4, r_memptr
    mov r5, r_sp
    // already at saved_register_file_hi
    stm r0!, {r1, r2, r3, r4, r5}
    bx  lr

.align 2
//9: .word z80a_resting_state
.global z80a_resting_state
z80a_resting_state:
// note this layout should match struct in z80a.h
_im: .word 0
_eipos: .word 0
_int_flags:
    .byte 0 // r_hi;
_iff1:
	.byte 0 // iff1;
	.byte 0 // iff2;
_halted:
	.byte 0 // halted;
// these Z80 registers don't have their own ARM reg
ir_register:
r_low: .byte 0
i: .byte 0
.byte 0
.byte 0
ix_register: .word 0
iy_register: .word 0
// location to save the other registers
saved_register_file_lo:
saved_r_pc:
.word 0 // r4 r_pc
.word 0 // r5 memory
.word 0 // r6 r_t
.word 0 // r7 r_af
// ----
saved_register_file_hi:
.word 0 // r8 r_bc
.word 0 // r9 r_de
.word 0 // r10 r_hl
.word 0 // r11 r_sp
.word 0 // r12 r_memptr

.word 0 // alt_af
.word 0 // alt_bc
.word 0 // alt_de
.word 0 // alt_hl
scratch: .word 0
step_hook_func: .word 0
is_active: .word 0 // active
#ifdef ENABLE_BREAKPOINT
bp_addr: .word 0
last_pc: .word 0
#endif

.macro dbg_check_breakpoint
#ifdef ENABLE_BREAKPOINT
    ldr r0, =z80a_resting_state
    ldr r1, [r0, #(bp_addr - z80a_resting_state)]
    cmp r_pc, r1
    bne 7f
    #if 0
    push {lr}
    // want to have data available
    bl  z80a_unload_state
    bl  z80a_breakpoint_hit
    bl  z80a_load_state
    pop {r0}
    mov lr, r0
    #else
    bkpt #0
    #endif
7:
    ldr r0, =z80a_resting_state
    str r_pc, [r0, #(last_pc - z80a_resting_state)]
#endif
.endm

.macro step_op_table_in_r_temp
#ifndef NO_USE_DOS
#error rom->Read(pc);
#endif
	#(this->*normal_opcodes[Fetch()])();
    fetch
    lsls r0, #1
    ldrh r0, [r_temp, r0]
    adds r0, r_temp
    blx r0
.endm

.macro step_op_table_in_r_temp_maybe_neg
#ifndef NO_USE_DOS
#error rom->Read(pc);
#endif
	#(this->*normal_opcodes[Fetch()])();
    fetch
    lsls r0, #1
    ldrh r0, [r_temp, r0]
    sxth r0, r0
    adds r0, r_temp
    blx r0
.endm

.align 1

.global z80a_step
.type z80a_step,%function
.thumb_func
z80a_step:
    push {lr}
    bl z80a_load_state
    dbg_check_breakpoint
    ldr r_temp, =op_table
    step_op_table_in_r_temp
    b z80a_unload_state
    pop {pc}

.section .data_z80a,"x"

// NOTE: general convention is to preserve anything other than r0-r1 in a function unless it is an explicit side effect

// r0 = address
// return in r0
// this is a one instruction thing with single 64K RAM, so inline it everywhere
//#ifndef USE_SINGLE_64K_MEMORY
//read8:
//    read8_internal r0
//    bx lr
//#endif

// r0 = address
// return in r0
ioread8:
    mov r1, r_t
    push {r3, lr}
    mov r3, r12
    push {r3}
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r3, frame_tacts
    adds r1, r3
#endif
    // todo caused by code being in data section
    ldr r2, =static_device_io_read
    blx r2
    pop {r3}
    mov r12, r3
    pop {r3, pc}

// r0 = address
// return value r0 address+1 in r1
read8inc:
    mov r1, r0
    read8inc_internal r1
    bx lr

// r0 = address
// return value in r0
read16:
    read16_internal_r0
    bx lr

// r0 = address
// return value r0 address+2 in r1
read16inc:
    mov r1, r0
    read16inc_internal_r1
    bx lr

// r0 = value
// r1 = address
.thumb_func
write8:
    write8_internal_r1
    bx lr

// r0 = value
// r1 = address
.thumb_func
iowrite8:
    mov r2, r_t
    push {r3, lr}
    mov r3, r12
    push {r3}
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r3, frame_tacts
    adds r2, r3
#endif
// todo caused by code being in data section
    ldr r3, =static_device_io_write
    blx r3
    pop {r3}
    mov r12, r3
    pop {r3, pc}

.ltorg

// r0 = value
// r1 = address
.thumb_func
write16:
    write16_internal_r1
    bx lr

.thumb_func
_push:
    mov r2, r_sp
    subs r2, #2
    uxth r2, r2
    mov r_sp, r2

    lsrs r1, r2, #1
    bcs 1f // misaligned

#ifndef USE_BANKED_MEMORY_ACCESS
    lsrs r1, r2, #14
    beq 3f
    strh r0, [memory, r2]
    bx lr

1:
    lsrs r1, r2, #14
    beq 2f
    strb r0, [memory, r2]
2:
    adds r2, #1
    uxth r2, r2
    lsrs r1, r2, #14
    beq 3f

    lsrs r0, #8
    strb r0, [memory, r2]
#else
    str r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    strh r0, [r1, r2]
    bx lr
1:
    str r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    ldr r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    strb r0, [r1, r2]
2:
    adds r2, #1
    str r2, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_ACCUM0_OFFSET)]
    uxth r2, r2
    ldr r1, [interp_base, #INTERP_OFFSET1(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldr r1, [r1]
    lsrs r0, #8
    strb r0, [r1, r2]
#endif
3:
    bx lr

.section .data_z80a_arith,"x"
.thumb_func
// Combined subs ands sbcs8 for size at small speed cost for sub
.thumb_func
sub8:
    // clear the z80 carry
    movs r1, #CF
    bics r_af, r1

.thumb_func
sbc8:
    // toggle the carry (ARM is reversed for subtract)
    movs r1, #CF
    eors r_af, r1

    lsrs r2, r_af, #8                // a
    lsls r2, #24                     // (a << 24)
    lsls r0, #24                     // (val << 24)

    push {r3}

    // perform subtraction

    // argggh on thumb this updares flags (it is adds r3, r2, #0)
    // todo this should no longer be the case with .unified
    mov r3, r2
    // set arm carry to !z80 carry
    lsrs r1, r_af, #1
    sbcs r3, r0                      // (a << 24) - ((val << 24) + c)

    // save away 4 arm flags in low nibble of r1
    mrs r1, apsr
    lsrs r1, #28

    // low nibble of a in high nibble of of r2
    lsls r2, r2, #4

    // low nibble of val in high nibble of r0
    lsls r0, #4

    // set arm carry to !z80 carry
    lsrs r_af, #1

    // set flags based on half carry
    sbcs r2, r0

    // set z80 flags based on saved arm flags
    // note arm carry flag is preserved across the next two instructions
    adr r2, flag_translate_sub_no_ZF
    ldrb r_af, [r2, r1]           // a = 0; f = corrsect scv (note z is not corrsect because of ffffff in low 24 bits

    // set the half carry (note we set when ARM carry is clear)
    bcs 1f
    adds r_af, #HF
1:
    // set N flag
    adds r_af, #NF
    // r1 = result
    lsrs  r1, r3, #24

    // need to set zero flag according to result
    bne  1f
    adds  r_af, #ZF
1:

    // set F3, F5 from result
    movs r3, #F3|F5
    ands r3, r1
    orrs r_af, r3

    // ... ands store the result in a
    lsls  r1, r1, #8
    orrs  r_af, r1

    pop {r3}
    bx lr

.thumb_func
neg8:
    lsrs r0, r_af, #8                // a
    lsls r0, #24

    // perform subtraction to set the flags
    negs r2, r0                      // -a

    mrs r_af, apsr
    adr r1, flag_translate_sub
    lsrs r_af, #28
    ldrb r_af, [r1, r_af]           // a = 0; f = corrsect szcv

    lsrs  r1, r2, #16
    orrs  r_af, r1                   // store result

    // set F3, F5 from result
    movs r2, #F3|F5
    lsrs r1, #8
    ands r1, r2
    orrs r_af, r1

    // ands low nibble of val in high nibble of r0
    lsls r0, #4

    // set flags based on half carry
    negs r1, r0

    // ARM c flag on subtract is opposite of Z80 one
    bcc 1f

    // H is now clear, N is set
    adds r_af, #NF
    bx  lr
1:
    // H is now set, N is set
    adds r_af, #HF|NF
    bx lr

.thumb_func
cp8:
    lsrs r2, r_af, #8                // a
    lsls r2, #24
    lsls r0, #24

    // perform subtraction to set the flags
    subs r1, r2, r0                  // a - val

    mrs r_af, apsr
    adr r1, flag_translate_sub
    lsrs r_af, #28
    ldrb r_af, [r1, r_af]           // a = 0; f = corrsect szcv

    lsrs  r1, r2, #16
    orrs  r_af, r1                   // restore a

    // set F3, F5 from val
    movs r1, #F3|F5
    rev r0, r0
    ands r1, r0
    orrs r_af, r1

    // restore low nibble of a in high nibble of of r1
    lsls r1, r2, #4
    // ands low nibble of val in high nibble of r0
    lsls r0, #28

    // set flags based on half carry
    subs r1, r0

    // ARM c flag on subtract is opposite of Z80 one
    bcc 1f

    // H is now clear, N is set
    adds r_af, #NF
    bx  lr
1:
    // H is now set, N is set
    adds r_af, #HF|NF
    bx lr

.thumb_func
adc16:
    mov r2, r_hl
    mvns r2, r2
    lsls r2, #16
    mvns r2, r2                      // (a << 16) + 0x0000ffff

    lsls r0, #16                     // (val << 16)

    push {r3}

    // perform addition

    // argggh on thumb this updares flags (it is adds r3, r2, #0)
    mov r3, r2
    // set arm carry to z80 carry
    lsrs r1, r_af, #1
    adcs r3, r0                      // (hl << 16) + (val << 16) + c + 0x0000ffff

    // save away 4 arm flags in low nibble of r1
    mrs r1, apsr
    lsrs r1, #28

    // low 3 nibbles of hl in high nibbles of of r2 ands 1s in bits 0 to 19
    lsls r2, r2, #4
    adds r2, #0xf

    // low 3 nibbles of val in high nibbles of r0
    lsls r0, #4

    // set arm carry to z80 carry
    lsrs r_af, #1

    // set flags based on half carry
    adcs r2, r0

    // set z80 flags based on saved arm flags
    // note arm carry flag is preserved across the next two instructions
    adr r2, flag_translate_add_no_ZF
    ldrb r2, [r2, r1]           // r2 = corrsect scv (note z is not corrsect because of ffffff in low 24 bits

    // set the half carry
    bcc 1f
    adds r2, #HF
1:

    lsrs r_af, #7
    lsls r_af, #8                // af now a:0
    orrs r_af, r2

    // r1 = result
    lsrs  r1, r3, #16

    // need to set zero flag according to result
    bne  1f
    adds  r_af, #ZF
1:

    // set F3, F5 from result
    lsrs r0, r1, #8
    movs r2, #F3|F5
    ands r2, r0

    orrs r_af, r2

    mov r_hl, r1

    pop {r3}
    bx lr

.thumb_func
sbc16:
    // toggle the carry (ARM is reversed for subtract)
    movs r1, #CF
    eors r_af, r1

    mov r2, r_hl
    lsls r2, #16                     // (hl << 16)
    lsls r0, #16                     // (val << 16)

    push {r3}

    // perform subtraction

    // argggh on thumb this updares flags (it is adds r3, r2, #0)
    mov r3, r2
    // set arm carry to !z80 carry
    lsrs r1, r_af, #1
    sbcs r3, r0                      // (hl << 16) - ((val << 16) + c)

    // save away 4 arm flags in low nibble of r1
    mrs r1, apsr
    lsrs r1, #28

    // low 3 nibbles of a in high 3 nibbles of of r2
    lsls r2, r2, #4

    // low nibble of val in high nibble of r0
    lsls r0, #4

    // set arm carry to !z80 carry
    lsrs r_af, #1

    // set flags based on half carry
    sbcs r2, r0

    // set z80 flags based on saved arm flags
    // note arm carry flag is preserved across the next two instructions
    adr r2, flag_translate_sub_no_ZF
    ldrb r2, [r2, r1]           // r2 = corrsect scv (note z is not corrsect because of ffffff in low 24 bits

    // set the half carry (note we set when ARM carry is clear)
    bcs 1f
    adds r2, #HF
1:

    lsrs r_af, #7
    lsls r_af, #8                // af now a:0
    orrs r_af, r2

    // set N flag
    adds r_af, #NF

    // r1 = result
    lsrs r1, r3, #16

    // need to set zero flag according to result
    bne 1f
    adds r_af, #ZF
1:
    // set F3, F5 from result hi byte
    lsrs r0, r1, #8
    movs r2, #F3|F5
    ands r2, r0
    orrs r_af, r2

    mov r_hl, r1

    pop {r3}
    bx lr

.thumb_func
add16:
    lsls r2, r1, #16
    lsls r0, #16

    push {r3}
    preserve_only_flags r3, (SF|ZF|PV)

    adds r3, r2, r0              // v0 + v1

    bcc 1f
    adds r_af, #CF
1:

    // restore low nibble of a in high nibble of of r1
    lsls r1, r2, #4
    // ands low nibble of val in high nibble of r0
    lsls r3, r0, #4

    // set flags based on half carry
    adds r1, r3

    bcc 2f

    movs r1, #HF
    orrs r_af, r1
2:

    pop {r3}

    // redo addition (cheaper than push/pop) ands we now have result in r0
    adds r0, r2
    lsrs r0, #16

    // set F3, F5 from result
    movs r1, #F3|F5
    lsrs r2, r0, #8
    ands r1, r2
    orrs r_af, r1

    bx lr

.align 2
// arm NZCV to z80 ZVC
flag_translate_sub:
    .byte 0  |      CF
    .byte 0  |      CF | PV
    .byte 0
    .byte 0  |           PV
    .byte 0  | ZF | CF
    .byte 0  | ZF | CF | PV
    .byte 0  | ZF
    .byte 0  | ZF |    | PV

    .byte SF |      CF
    .byte SF |      CF | PV
    .byte SF
    .byte SF |         | PV
    .byte SF | ZF | CF
    .byte SF | ZF | CF | PV
    .byte SF | ZF
    .byte SF | ZF |    | PV

.align 2
// arm NZCV to z80 SVC
flag_translate_sub_no_ZF:
    .byte 0  | CF
    .byte 0  | CF | PV
    .byte 0
    .byte 0       | PV
    .byte 0  | CF
    .byte 0  | CF | PV
    .byte 0
    .byte 0       | PV

    .byte SF | CF
    .byte SF | CF | PV
    .byte SF
    .byte SF      | PV
    .byte SF | CF
    .byte SF | CF | PV
    .byte SF
    .byte SF      | PV

// Combined adds ands adcs8 for size at small speed cost for add
.thumb_func
add8:
    // clear the z80 carry
    movs r1, #CF
    bics r_af, r1

.thumb_func
adc8:
    lsrs r2, r_af, #8                // a
    mvns r2, r2
    lsls r2, #24
    mvns r2, r2                      // (a << 24) + 0x00ffffff

    lsls r0, #24                     // (val << 24)

    push {r3}

    // perform addition

    // argggh on thumb this updares flags (it is adds r3, r2, #0)
    mov r3, r2
    // set arm carry to z80 carry
    lsrs r1, r_af, #1
    adcs r3, r0                      // (a << 24) + (val << 24) + c + 0x00ffffff

    // save away 4 arm flags in low nibble of r1
    mrs r1, apsr
    lsrs r1, #28

    // low nibble of a in high nibble of of r2 ands 1s in bits 0 to 27
    lsls r2, r2, #4
    adds r2, #0xf

    // low nibble of val in high nibble of r0
    lsls r0, #4

    // set arm carry to z80 carry
    lsrs r_af, #1

    // set flags based on half carry
    adcs r2, r0

    // set z80 flags based on saved arm flags
    // note arm carry flag is preserved across the next two instructions
    adr r2, flag_translate_add_no_ZF
    ldrb r_af, [r2, r1]           // a = 0; f = corrsect scv (note z is not corrsect because of ffffff in low 24 bits

    // set the half carry
    bcc 1f
    adds r_af, #HF
1:

    // r1 = result
    lsrs  r1, r3, #24

    // need to set zero flag according to result
    bne  1f
    adds  r_af, #ZF
1:

    // set F3, F5 from result
    movs r3, #F3|F5
    ands r3, r1
    orrs r_af, r3

    // ... ands store the result in a
    lsls  r1, r1, #8
    orrs  r_af, r1

    pop {r3}
    bx lr

.align 2
flag_translate_add_no_ZF:
    .byte 0
    .byte 0       | PV
    .byte 0  | CF
    .byte 0  | CF | PV
    .byte 0
    .byte 0       | PV
    .byte 0  | CF
    .byte 0  | CF | PV

    .byte SF
    .byte SF      | PV
    .byte SF | CF
    .byte SF | CF | PV
    .byte SF
    .byte SF      | PV
    .byte SF | CF
    .byte SF | CF | PV

.thumb_func
set_af35_special_r_temp:
    // (tempbyte & F3) + ((tempbyte << 4) & F5);
    movs r0, #F3
    ands r0, r_temp
    orrs r_af, r0
    lsrs r0, r_temp, #2
    bcc 1f
    movs r0, #F5
    orrs r_af, r0
1:
    bx  lr

.section .data_z70a_op,"x"
.align 1
.thumb_func
// instruction prefix hell!!
opDDFD:
/**
    byte op1; // last DD/FD prefix
	do
	{
		op1 = opcode;
		opcode = Fetch();
	} while((opcode | 0x20) == 0xFD); // opcode == DD/FD
*/
1:
    mov r1, r0
    fetch
    cmp r0, #0xed
    bne 4f
    // ED
    ldr r0, =oped
    bx  r0
4:
    cmp r0, #0xdd
    beq 1b
    cmp r0, #0xfd
    beq 1b
    ldr r2, =z80a_resting_state
    ldr r_temp, =opxy_table
    cmp r1, #0xfd
    beq 3f
    // DD
    cmp r0, #0xcb
    bne 4f
    ldr r1, [r2, #(ix_register - z80a_resting_state)]
    read8inc_internal r_pc
    sxtb r0, r0
    adds r0, r1
    mov r_memptr, r0
    b 2f
4:
    lsls r0, #1
    ldrh r0, [r_temp, r0]
    sxth r0, r0
    adds r0, r_temp
    // note r_ixy == r_temp so we can't do this above
    ldr r_ixy, [r2, #(ix_register - z80a_resting_state)]
    push {lr}
    blx r0
    ldr r2, =z80a_resting_state
    str r_ixy, [r2, #(ix_register - z80a_resting_state)]
    pop {pc}
3:
    // FD
    cmp r0, #0xcb
    bne 4f
    ldr r1, [r2, #(iy_register - z80a_resting_state)]
    read8inc_internal r_pc
    sxtb r0, r0
    adds r0, r1
    mov r_memptr, r0
    b 2f
4:
    lsls r0, #1
    ldrh r0, [r_temp, r0]
    sxth r0, r0
    adds r0, r_temp
    // note r_ixy == r_temp so we can't do this above
    ldr r_ixy, [r2, #(iy_register - z80a_resting_state)]
    push {lr}
    blx r0
    ldr r2, =z80a_resting_state
    str r_ixy, [r2, #(iy_register - z80a_resting_state)]
    pop {pc}
2:
    // (FD/DD)CB
	ldr  r_temp, =opli_m35_table
	// not doing fetch here because of something about not incrementing r (although we don't do that anyway atm)
    read8inc_internal r_pc
    // this is rolled into the ddcb/ddcbit functions
    // adds r_t, #4

	lsrs  r2, r0, #3
	lsls  r2, #1
	ldrh r2, [r_temp, r2]
	sxth r2, r2
	adds  r2, r_temp

	ldr  r_temp, =opddcb_X_table
	lsrs  r1, r0, #7
	// arg no single test for this flag combo; basically this (with the shift above) is 0x40 == (r0 & 0xc0) i.e. is this bitmem()
	bcc  1f
	bne  1f
	// note that bitmem has different wrapper functions that don't update memory (ands increment t differently)
	adds  r_temp, #(opddcb_bitX_table - opddcb_X_table)
1:
	movs  r1, #7
	ands  r0, r1
	lsls  r0, #1
	ldrh r1, [r_temp, r0]
	sxth r1, r1
	adds  r1, r_temp
	bx   r1
.ltorg

/**

	if(opcode == 0xCB)
	{
		dword ptr; // pointer to DDCB operands
		ptr = ((op1 == 0xDD) ? ix : iy) + (signed char)ReadInc(pc);
		memptr = ptr;
		// DDCBnnXX,FDCBnnXX increment R by 2, not 3!
		opcode = ReadInc(pc);
		t += 4;
		byte v = (this->*logic_ix_opcodes[opcode])(Read(ptr));
		if((opcode & 0xC0) == 0x40)// bit n,rm
		{
			t += 8;
			return;
		}
		// select destination register for shift/res/set
		(this->*reg_offset[opcode & 7]) = v; // ???
		Write(ptr, v);
		t += 11;
		return;
	}
	if(opcode == 0xED)
	{
		OpED();
		return;
	}
	// one prefix: DD/FD
	op1 == 0xDD ? (this->*ix_opcodes[opcode])() : (this->*iy_opcodes[opcode])();
	*/

.section .data_z80a_inc,"x"
.thumb_func
inc8:
    preserve_only_flags r1, CF
    adr     r1, _incf
    ldrb    r1, [r1, r0]
    orrs     r_af, r1
    adds     r0, #1
    uxtb    r0, r0
    bx      lr

.section .data_z80a_dec,"x"
.thumb_func
dec8:
    preserve_only_flags r1, CF
    adr     r1, _decf
    ldrb    r1, [r1, r0]
    orrs     r_af, r1
    subs     r0, #1
    uxtb    r0, r0
    bx      lr

.section .data_z80a_logic,"x"
.thumb_func
ands8:
    // r0 is value to ands in low 8
    lsrs     r_af, #8
    ands     r_af, r0
    adr     r1, _log_f
    ldrb    r0, [r1, r_af]
    lsls     r_af, #8
    movs     r1, #HF
    orrs     r_af, r0
    orrs     r_af, r1
    bx      lr

.thumb_func
or8:
    // r0 is value to ands in low 8
    lsrs     r_af, #8
    orrs     r_af, r0
    adr     r1, _log_f
    ldrb    r0, [r1, r_af]
    lsls     r_af, #8
    orrs     r_af, r0
    bx      lr

.thumb_func
xor8:
    // r0 is value to ands in low 8
    lsrs     r_af, #8
    eors     r_af, r0
    adr     r1, _log_f
    ldrb    r0, [r1, r_af]
    lsls     r_af, #8
    orrs     r_af, r0
    bx      lr

.thumb_func
rlc8:
    lsrs     r_af, #8
    lsls     r_af, #8

    movs     r1, #0
    lsls     r0, #25
    bcc     1f
    movs     r1, #1
    orrs     r_af, r1
1:
    lsrs     r0, #24
    orrs     r0, r1

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
rl8:
    movs     r1, #CF
    ands     r1, r_af
    lsrs     r_af, #8
    lsls     r_af, #8

    lsls     r0, #25
    bcc     1f
    movs     r2, #CF
    orrs     r_af, r2
1:
    lsrs     r0, #24
    orrs     r0, r1

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
rr8:
    movs     r1, #CF
    ands     r1, r_af
    lsrs     r_af, #8
    lsls     r_af, #8

    lsrs     r0, #1
    bcc     1f
    movs     r2, #CF
    orrs     r_af, r2
1:
    lsls     r1, #7
    orrs     r0, r1

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
sla8:
    lsrs     r_af, #8
    lsls     r_af, #8

    lsls     r0, #25
    bcc     1f
    movs     r1, #CF
    orrs     r_af, r1
1:
    lsrs     r0, #24

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
sli8:
    movs     r1, #CF
    lsrs     r_af, #8
    lsls     r_af, #8

    lsls     r0, #25
    bcc     1f
    orrs     r_af, r1
1:
    lsrs     r0, #24
    orrs     r0, r1

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr


.thumb_func
sra8:
    lsrs     r_af, #8
    lsls     r_af, #8

    lsls     r0, #24
    asrs     r0, #25

    bcc     1f
    movs     r1, #CF
    orrs     r_af, r1
1:
    uxtb    r0, r0

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
srl8:
    lsrs     r_af, #8
    lsls     r_af, #8

    lsrs     r0, #1
    bcc     1f
    movs     r1, #CF
    orrs     r_af, r1
1:
    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.thumb_func
rrc8:
    lsrs     r_af, #8
    lsls     r_af, #8

    lsrs     r0, #1
    bcc     1f
    movs     r1, #CF
    orrs     r_af, r1
    movs     r1, #128
    orrs     r0, r1
1:

    adr     r1, _log_f
    ldrb    r1, [r1, r0]
    orrs     r_af, r1

    bx      lr

.section .data_z80a_inc
.align 2
_incf:
    .byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08
    .byte	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x10
    .byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08
    .byte	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x30
    .byte	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28
    .byte	0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x30
    .byte	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28
    .byte	0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x10
    .byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08
    .byte	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x10
    .byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08
    .byte	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x30
    .byte	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28
    .byte	0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x30
    .byte	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28
    .byte	0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x94
    .byte	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88
    .byte	0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x90
    .byte	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88
    .byte	0x88,0x88,0x88,0x88,0x88,0x88,0x88,0xb0
    .byte	0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa8
    .byte	0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xb0
    .byte	0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa8
    .byte	0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0x90
    .byte	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88
    .byte	0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x90
    .byte	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88
    .byte	0x88,0x88,0x88,0x88,0x88,0x88,0x88,0xb0
    .byte	0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa8
    .byte	0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xb0
    .byte	0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa8
    .byte	0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0xa8,0x50

.section .data_z80a_dec
.align 2
_decf:
    .byte	0xba,0x42,0x02,0x02,0x02,0x02,0x02,0x02
    .byte	0x02,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a
    .byte	0x1a,0x02,0x02,0x02,0x02,0x02,0x02,0x02
    .byte	0x02,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a
    .byte	0x1a,0x22,0x22,0x22,0x22,0x22,0x22,0x22
    .byte	0x22,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a
    .byte	0x3a,0x22,0x22,0x22,0x22,0x22,0x22,0x22
    .byte	0x22,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a
    .byte	0x3a,0x02,0x02,0x02,0x02,0x02,0x02,0x02
    .byte	0x02,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a
    .byte	0x1a,0x02,0x02,0x02,0x02,0x02,0x02,0x02
    .byte	0x02,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a
    .byte	0x1a,0x22,0x22,0x22,0x22,0x22,0x22,0x22
    .byte	0x22,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a
    .byte	0x3a,0x22,0x22,0x22,0x22,0x22,0x22,0x22
    .byte	0x22,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a,0x2a
    .byte	0x3e,0x82,0x82,0x82,0x82,0x82,0x82,0x82
    .byte	0x82,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a
    .byte	0x9a,0x82,0x82,0x82,0x82,0x82,0x82,0x82
    .byte	0x82,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a
    .byte	0x9a,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2
    .byte	0xa2,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
    .byte	0xba,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2
    .byte	0xa2,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
    .byte	0xba,0x82,0x82,0x82,0x82,0x82,0x82,0x82
    .byte	0x82,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a
    .byte	0x9a,0x82,0x82,0x82,0x82,0x82,0x82,0x82
    .byte	0x82,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a
    .byte	0x9a,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2
    .byte	0xa2,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
    .byte	0xba,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2,0xa2
    .byte	0xa2,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa

.global _log_f

.section .data_z80a_logic
.align 2
_log_f:
    .byte	0x44,0x00,0x00,0x04,0x00,0x04,0x04,0x00
    .byte	0x08,0x0c,0x0c,0x08,0x0c,0x08,0x08,0x0c
    .byte	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04
    .byte	0x0c,0x08,0x08,0x0c,0x08,0x0c,0x0c,0x08
    .byte	0x20,0x24,0x24,0x20,0x24,0x20,0x20,0x24
    .byte	0x2c,0x28,0x28,0x2c,0x28,0x2c,0x2c,0x28
    .byte	0x24,0x20,0x20,0x24,0x20,0x24,0x24,0x20
    .byte	0x28,0x2c,0x2c,0x28,0x2c,0x28,0x28,0x2c
    .byte	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04
    .byte	0x0c,0x08,0x08,0x0c,0x08,0x0c,0x0c,0x08
    .byte	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00
    .byte	0x08,0x0c,0x0c,0x08,0x0c,0x08,0x08,0x0c
    .byte	0x24,0x20,0x20,0x24,0x20,0x24,0x24,0x20
    .byte	0x28,0x2c,0x2c,0x28,0x2c,0x28,0x28,0x2c
    .byte	0x20,0x24,0x24,0x20,0x24,0x20,0x20,0x24
    .byte	0x2c,0x28,0x28,0x2c,0x28,0x2c,0x2c,0x28
    .byte	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84
    .byte	0x8c,0x88,0x88,0x8c,0x88,0x8c,0x8c,0x88
    .byte	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80
    .byte	0x88,0x8c,0x8c,0x88,0x8c,0x88,0x88,0x8c
    .byte	0xa4,0xa0,0xa0,0xa4,0xa0,0xa4,0xa4,0xa0
    .byte	0xa8,0xac,0xac,0xa8,0xac,0xa8,0xa8,0xac
    .byte	0xa0,0xa4,0xa4,0xa0,0xa4,0xa0,0xa0,0xa4
    .byte	0xac,0xa8,0xa8,0xac,0xa8,0xac,0xac,0xa8
    .byte	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80
    .byte	0x88,0x8c,0x8c,0x88,0x8c,0x88,0x88,0x8c
    .byte	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84
    .byte	0x8c,0x88,0x88,0x8c,0x88,0x8c,0x8c,0x88
    .byte	0xa0,0xa4,0xa4,0xa0,0xa4,0xa0,0xa0,0xa4
    .byte	0xac,0xa8,0xa8,0xac,0xa8,0xac,0xac,0xa8
    .byte	0xa4,0xa0,0xa0,0xa4,0xa0,0xa4,0xa4,0xa0
    .byte	0xa8,0xac,0xac,0xa8,0xac,0xa8,0xa8,0xac

.section .data_z80a

// note this is down here to be close to op_table
.global z80a_update
.type z80a_update,%function
.thumb_func
// run until end of frame (t <= frame_tacts) or (t <= 0 if USE_Z80_ARM_OFFSET_T - saves a reg)
z80a_update:
    push {r4-r7,lr}
    mov r0, r8
    mov r1, r9
    mov r2, r10
    mov r3, r11
    push {r0-r3}

    bl z80a_load_state

    ldr r1, =z80a_resting_state
    ldrb r0, [r1, #(_halted - z80a_resting_state)]

    //if(!iff1 && halted)
    cmp r0, #0
    bne irqcheck_instruction_loop_test
    ldrb r0, [r1, #(_iff1 - z80a_resting_state)]
    beq irqcheck_instruction_loop_test
    // return
    b 9f
	// INT check separated from main Z80 loop to improve emulation speed
	//while(t < int_len)
	//{
irqcheck_instruction_loop:
    //	if(iff1 && t != eipos) // int enabled in CPU not issued after EI
    //	{
    //		Int();
    //		break;
    //	}
    ldrb r0, [r1, #(_iff1 - z80a_resting_state)]
    cmp r0, #0
    beq 3f

    ldr r0, [r1, #(_eipos - z80a_resting_state)]
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r2, frame_tacts
    subs r0, r2
#endif
    cmp r_t, r0
    beq 3f

    bl z80a_interrupt
    b 2f
3:

    //	Step();
    dbg_check_breakpoint
    adr r_temp, op_table
    step_op_table_in_r_temp

    ldr r1, =z80a_resting_state

    //if(halted)
    //		break;
    ldrb r0, [r1, #(_halted - z80a_resting_state)]
    cmp r0, #0
    bne 2f

irqcheck_instruction_loop_test:
    // note r1 should have z80a_resting_state at this point (even for first iteration)
    // }
#ifdef USE_Z80_ARM_OFFSET_T
    ldr r2, frame_tacts
    adds r2, r_t
    cmp r2, #32
#else
    cmp r_t, #32
#endif
    blt irqcheck_instruction_loop

2:
    // eipos = -1
    movs r0, #0
    mvns r0, r0
    str r0, [r1, #(_eipos - z80a_resting_state)]

hook_instruction_loop:
    ldr r0, [r1, #(step_hook_func - z80a_resting_state)]
    cmp r0, #0
    beq main_instruction_loop_test
    bl z80a_unload_state
    ldr r1, =z80a_resting_state
    ldr r0, [r1, #(step_hook_func - z80a_resting_state)]

    // call hook func
    blx r0
    bl z80a_load_state

    dbg_check_breakpoint
    adr r_temp, op_table
    step_op_table_in_r_temp
    ldr r1, =z80a_resting_state
#ifdef USE_Z80_ARM_OFFSET_T
    cmp r_t, #0
#else
    ldr r0, frame_tacts
    cmp r_t, r0
#endif
    blt hook_instruction_loop
    b 8f

main_instruction_loop:
    dbg_check_breakpoint
    adr r_temp, op_table
    step_op_table_in_r_temp
main_instruction_loop_test:
#ifdef USE_Z80_ARM_OFFSET_T
    cmp r_t, #0
#else
    ldr r0, frame_tacts
    cmp r_t, r0
#endif
    blt main_instruction_loop

8:  // finished; restore state
    ldr r0, frame_tacts
    subs r_t, r0
    ldr r1, =z80a_resting_state
    ldr r2, [r1, #(_eipos - z80a_resting_state)]
    subs r2, r0
    str r2, [r1, #(_eipos - z80a_resting_state)]

    bl z80a_unload_state

9:  // return
    pop {r0-r3}
    mov r8, r0
    mov r9, r1
    mov r10, r2
    mov r11, r3
    pop {r4-r7, pc}

.align 2
.ltorg
// todo externalize
frame_tacts: .word 71680

#include "z80khan_gen.S"