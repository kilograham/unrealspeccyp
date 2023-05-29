/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

.cpu cortex-m0
.thumb
.syntax divided
// todo corret volume ramps .. pretty cheap really

#define OFFSET_BITS_A03X 0
#define OFFSET_BITS_B14X 1
#define OFFSET_BITS_C25X 2
#define OFFSET_BIT_N 3

#define OFFSET_CH_F_A 6
#define OFFSET_CH_F_B 8
#define OFFSET_CH_F_C 10
#define OFFSET_CH_F_N 12
#define OFFSET_CH_F_E 14

#define OFFSET_A_OUTPUT 16
#define OFFSET_B_OUTPUT 18
#define OFFSET_C_OUTPUT 20

#define OFFSET_A_VOL 22
#define OFFSET_B_VOL 24
#define OFFSET_C_VOL 26
#define OFFSET_E_VOL 28
// must be next to e_vol
#define OFFSET_DE_VOL 30

#define OFFSET_MASTER_VOLUME 32
#define OFFSET_NS 36

// counter state 16 bytes
#define OFFSET_COUNTER_STATE 48
#define OFFSET_T_PAIRS 48
#define OFFSET_TA 48
#define OFFSET_TB 50
#define OFFSET_TC 52
#define OFFSET_TM 54
#define OFFSET_TE 56
#define OFFSET_TT 58
#define OFFSET_10001 60

// sample state 16 bytes
#define OFFSET_SAMPLE_STATE 64
#define OFFSET_FRAC_MAJOR 64
#define OFFSET_FRAC_MINOR 68
#define OFFSET_FRAC_ACCUM 72
#define OFFSET_OVER_C_VALUE 76
#define OFFSET_OVER_REM 80
#define OFFSET_OVER_ACCUM 84

#define OFFSET_WAKEUPS 88

#define OFFSET_CH_VOL 0
#define OFFSET_CH_VALUE 0


#define r_state r0

#define r_t_ab r1
#define r_sample_ptr r1

#define r_t_cn r2
#define r_frac_major r2

#define r_t_et r3
#define r_frac_minor r3

#define r_frac_accum r4

#define r_over_c_value r5

#define r_over_rem r6

#define r_hi_counter_loop_breakout r8

#define r_hi_saved_sample_ptr r10
// r6, tmp
// r7 0x10001
#.section .data_fast_ay,"x"

//#define SANIT_AY

.macro debug_break
//    bkpt #0
.endm

.macro check_over_rem
#ifdef SANIT_AY
    cmp r_over_rem, #0
    bge over_rem_ok\@
    bkpt #0
over_rem_ok\@:
#endif
.endm

buffer_full1:
#ifdef SANIT_AY
    bkpt #0 // shouldn't happen any more (but still does very rarely atm :-( )
#endif
//    mov r10, r1
//    mov r1, r_state
//    add r1, #OFFSET_SAMPLE_STATE + 8
//    stmia r1!, {r_frac_accum, r_over_c_value, r_over_rem}
buffer_full2:
#ifdef SANIT_AY
    bkpt #0 // shouldn't happen any more (but still does very rarely atm :-( )
#endif

    mov r0, #0
    str r0, [r1, #OFFSET_OVER_REM] // no more samples

all_done:
    debug_break
    mov r0, r10
    pop {r1, r4-r7}
    sub r0, r1
    lsr r0, #1
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    pop {r4-r7, pc}

.align 2
.global fast_ay
.type fast_ay,%function
.thumb_func
fast_ay:
    push {r4-r7, lr}
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    push {r1, r4-r7}

    add r2, r2
    add r2, r1
    mov r9, r2

    // save original sample_ptr
    mov r10, r1
    // note we now expect that over_rem is always 0 on entry
#ifdef SANIT_AY
    ldr r7, [r_state, #OFFSET_OVER_REM]
    cmp r7, #0
    beq 1f
    bkpt #0
1:
#endif
    // so we jump straight to counter loop (kept code in
    // this order because it makes branches in range)
    b counter_loop_enter

flush_samples:
    debug_break

// r0 - state
// r1 - sample_ptr
// r2 - frac_major
// r3 - frac_minor
// r4 - frac_accum
// r5 - oc_v (aa00 0000 0000 0000 bbbb bbbb bbbb bbbb - oc = oversample_count 0-3, v = value to write)
// r6 - over_rem
// r7 -
// r8 - next_value (or -1)
// r9 - end of sample buffer
// r10 - saved sample ptr
// r11 - saved done flag

    mov r7, r_state
    add r7, #OFFSET_SAMPLE_STATE
    ldmia r7!, {r_frac_major, r_frac_minor, r_frac_accum, r_over_c_value, r_over_rem}
    check_over_rem
//    bkpt #0

    cmp r_over_rem, #0
    beq no_flush

//    debug_break

    lsr r7, r_over_c_value, #30
    beq sample_aligned


    mov r8, r2
    ldr r2, [r_state, #OFFSET_OVER_ACCUM]

2:
    sub r_over_rem, #1
1:
    add r2, r_over_c_value
    add r7, #1
    cmp r7, #4
    beq 2f
    cmp r_over_rem, #0
    bne 2b
2:
    lsl r_over_c_value, #2
    lsr r_over_c_value, #2
    lsl r7, #30
    orr r_over_c_value, r7
    cmp r7, #0
    bne no_full_sample

    lsr r2, #2 // (poorly) filtered sample

    check_over_rem

    sub r_frac_accum, r_frac_minor
    bge 1f
    // r2 is r_frac_major, so we need to reload into now spare reg r7
    mov r7, r8
    add r_frac_accum, r7
    strh r2, [r1]
    add r1, #2
    cmp r1, r9
    bhs buffer_full1
1:

    mov r2, #0
    str r2, [r_state, #OFFSET_OVER_ACCUM]

no_full_sample:
    mov r2, r8
sample_aligned:
1:
    sub r_over_rem, #4
    bmi 3f
2:
    sub r_frac_accum, r_frac_minor
    bge 1b
    add r_frac_accum, r_frac_major
    strh r_over_c_value, [r1]
    add r1, #2
    check_over_rem
    cmp r1, r9
    bhs buffer_full2
    sub r_over_rem, #4
    bpl 2b
3:
    // save sample ptr
    mov r10, r1

    debug_break

    // save and mask off top 2 bits
    lsr r7, r_over_c_value, #30
    lsl r7, r7, #30
    eor r_over_c_value, r7

    // set r1 to % 4 (i.e. what is left)
    lsl r_over_rem, #30
    lsr r1, r_over_rem, #30

    beq no_fraction

#ifdef SANIT_AY
    push {r6}
    uxth r6, r_over_c_value
    cmp r6, r_over_c_value
    beq 1f
    bkpt #0
1:
    pop {r6}
#endif
    // calculate accumulation based on remaining bits
    mul r1, r_over_c_value

    // use r_over_c_value as temp here
    ldr r_over_c_value, [r_state, #OFFSET_OVER_ACCUM]
    add r1, r_over_c_value
#ifdef SANIT_AY
    push {r6}
    lsl r6, r1, #14
    lsr r6, r6, #14
    cmp r6, r1
    beq 1f
    bkpt #0
1:
    pop {r6}
#endif
    str r1, [r_state, #OFFSET_OVER_ACCUM]
no_fraction:

    ldr r3, =current_voltab
    ldr r3, [r3]

    // recalculate sample value
    ldr r1, [r_state, #OFFSET_A_OUTPUT] // AB
    lsl r1, #1
    lsr r2, r1, #16 // B*2
    uxth r1, r1 // A*2
    ldrh r1, [r3, r1]
    ldrh r2, [r3, r2]
    add r1, r2
    ldrh r_over_c_value, [r_state, #OFFSET_C_OUTPUT]
    lsl r_over_c_value, #1
    ldrh r_over_c_value, [r3, r_over_c_value]
    add r_over_c_value, r1
    ldr r1, [r_state, #OFFSET_MASTER_VOLUME]
    mul r_over_c_value, r1
    lsr r_over_c_value, #6 + 4 // todo could be part of volume table

    orr r_over_c_value, r7
    add r_over_c_value, r_over_rem
#ifdef SANIT_AY
    bcc 1f
    bkpt #0 // if there were enough remaining to fill a whole oversample bucket, we should have handled it at the top
1:
#endif
    mov r_over_rem, #0

    mov r1, r_state
    add r1, #OFFSET_FRAC_ACCUM
    check_over_rem
    //bkpt #0
    stmia r1!, {r_frac_accum, r_over_c_value, r_over_rem}

no_flush:
    // tt
    mov r6, r11
    cmp r6, #0
    beq all_done

    debug_break
counter_loop_enter:
    // simple for now just load register bank
    mov r6, r_state
    add r6, #OFFSET_COUNTER_STATE
    ldmia r6!, {r_t_ab, r_t_cn, r_t_et, r7}

    lsr r4, r_t_et, #16
#ifdef SANIT_AY
    bne 1f
    bkpt #0
1:
#endif
    mov r_hi_counter_loop_breakout, r7 // set to 0x10001 which is bigger than samples
    // r0 - state
    // r1 - t_ab
    // r2 - t_cn
    // r3 - t_ey
    // r4 - tt original
    // r5 - scratch
    // r6 - scratch
    // r7 - 0x10001
    // r8 - breakout_flag
    // r9 - end of sample buffer
    // r10 - saved sample pltr

counter_loop:
    sub r_t_ab, r7
    lsl r6, r_t_ab, #16
    beq a_done
.thumb_func
check_b:
    lsr r6, r_t_ab, #16
    beq b_done
.thumb_func
check_c:
    sub r_t_cn, r7
    lsl r6, r_t_cn, #16
    beq c_done
.thumb_func
check_n:
    lsr r6, r_t_cn, #16
    beq n_done
.thumb_func
check_e:
    sub r_t_et, r7
    lsl r6, r_t_et, #16
    beq e_done
.thumb_func
check_t:
    lsr r6, r_t_et, #16
    beq save_counter_state
    cmp r6, r_hi_counter_loop_breakout
    blo counter_loop

    // here we broke out because something changed

save_counter_state:
    debug_break

    // get number of cycles
    lsr r7, r_t_et, #16
    sub r4, r7
#ifdef SANIT_AY
    bhs 1f
    bkpt #0
1:
#endif

    // save tt
    mov r11, r6

#ifdef SANIT_AY
    ldr r5, [r_state, #OFFSET_OVER_REM]
    cmp r5, #0
    beq 1f
    bkpt #0 // wrong
1:
#endif
    str r4, [r_state, #OFFSET_OVER_REM]

    mov r5, r_state
    add r5, #OFFSET_COUNTER_STATE
    stmia r5!, {r_t_ab, r_t_cn, r_t_et}

    // load sample ptr
    mov r1, r10

    b flush_samples

//    add r4, r_state, #4
//    ldr r6, [r4, #OFFSET_WAKEUPS - 4]
//    add r6, #1
//    str r6, [r4, #OFFSET_WAKEUPS - 4]


zero_fa:
    // set ta to 0xffff
    sub r6, r7, #2
    add r_t_ab, r6
    b check_b
a_done:
    mov r8, r6
    ldrh r6, [r_state, #OFFSET_CH_F_A]
    // if the frequency is 0 we do nothing - and we will wrap to
    cmp r6, #0
    beq zero_fa
    add r6, #1
    add r_t_ab, r6
    ldrb r6, [r_state, #OFFSET_BITS_A03X]
    mov r5, #8
    eor r6, r5
    strb r6, [r_state, #OFFSET_BITS_A03X]
    ldr r6, =check_b
    mov lr, r6
    b update_a

zero_fb:
    // set tb to 0xffff
    sub r_t_ab, r7
    add r_t_ab, #1
    b check_c
b_done:
    mov r8, r6
    ldrh r6, [r_state, #OFFSET_CH_F_B]
    // if the frequency is 0 we do nothing - and we will wrap to
    lsl r6, #16
    beq zero_fb
    add r_t_ab, r6
    add r_t_ab, r7
    sub r_t_ab, #1
    ldrb r6, [r_state, #OFFSET_BITS_B14X]
    mov r5, #8
    eor r6, r5
    strb r6, [r_state, #OFFSET_BITS_B14X]
    ldr r6, =check_c
    mov lr, r6
    b update_b

zero_fc:
    // set tc to 0xffff
    sub r6, r7, #2
    add r_t_cn, r6
    b check_n
c_done:
    mov r8, r6
    ldrh r6, [r_state, #OFFSET_CH_F_C]
    // if the frequency is 0 we do nothing - and we will wrap to
    cmp r6, #0
    beq zero_fc
    add r6, #1
    add r_t_cn, r6
    ldrb r6, [r_state, #OFFSET_BITS_C25X]
    mov r5, #8
    eor r6, r5
    strb r6, [r_state, #OFFSET_BITS_C25X]
    ldr r6, =check_n
    mov lr, r6
    b update_c

zero_fn:
    // set tn to 0xffff
    sub r_t_cn, r7
    add r_t_cn, #1
    b check_e
n_done:
    mov r8, r6
    ldrh r6, [r_state, #OFFSET_CH_F_N]
    // if the frequency is 0 we do nothing - and we will wrap to
    lsl r6, #16
    beq zero_fn
    add r_t_cn, r6
    add r_t_cn, r7
    sub r_t_cn, #1

    // ns = (ns * 2 + 1) ^ (((ns >> 16) ^ (ns >> 13)) & 1),
    // bitN = 0 - ((ns >> 16) & 1);

    ldr r5, [r_state, #OFFSET_NS]
    lsl r6, r5, #3
    eor r6, r5
    lsl r6, #15
    lsr r6, #31
    add r5, r5
    add r5, #1
    eor r5, r6
    str r5, [r_state, #OFFSET_NS]
    lsl r5, #15
    lsr r5, #31
    strb r5, [r_state, #OFFSET_BIT_N]

    // todo inline?
    bl update_a
    bl update_b
    bl update_c
    b check_e

zero_fe:
    // set te to 0xffff
    sub r6, r7, #2
    add r_t_et, r6
    b check_t
e_done:
    mov r8, r6
    ldrh r6, [r_state, #OFFSET_CH_F_E]
    // if the frequency is 0 we do nothing - and we will wrap to
    cmp r6, #0
    beq zero_fe
    add r6, #1
    add r_t_et, r6

    // update envelope
    ldr r5, [r_state, #OFFSET_E_VOL]
    lsr r6, r5, #16 // DE_VOL
    add r5, r6
    lsl r5, #27
    lsr r5, #27
    beq 1f
    ldr r6, =envelope_handler
    ldr r6, [r6]
    blx r6
    strh r5, [r_state, #OFFSET_E_VOL]
1:

    // todo inline?
    bl update_a
    bl update_b
    bl update_c
    b check_t

// update net effect of channel a
update_a:
    ldrb r6, [r_state, #OFFSET_BITS_A03X]
    lsr r5, r6, #5
    bcs 1f
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_A_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_A_OUTPUT]
    bx lr
1:
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic - 16
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_E_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_A_OUTPUT]
    bx lr

// update net effect of channel b
update_b:
    ldrb r6, [r_state, #OFFSET_BITS_B14X]
    lsr r5, r6, #5
    bcs 1f
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_B_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_B_OUTPUT]
    bx lr
1:
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic - 16
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_E_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_B_OUTPUT]
    bx lr

// update net effect of channel c
update_c:
    ldrb r6, [r_state, #OFFSET_BITS_C25X]
    lsr r5, r6, #5
    bcs 1f
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_C_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_C_OUTPUT]
    bx lr
1:
    ldrb r5, [r_state, #OFFSET_BIT_N]
    add r6, r5
    ldr r5, =channel_logic - 16
    ldrb r6, [r5, r6]
    ldrh r5, [r_state, #OFFSET_E_VOL]
    mul r6, r5
    strh r6, [r_state, #OFFSET_C_OUTPUT]
    bx lr

.thumb_func
envelope_wrap:
    // already wrapped
    bx lr

.thumb_func
envelope_floor:
    mov r5, #0
    strh r5, [r_state, #OFFSET_DE_VOL]
    bx lr

.thumb_func
envelope_ceil:
    mov r5, #0
    strh r5, [r_state, #OFFSET_DE_VOL]
    mov r5, #31
    bx lr

.thumb_func
envelope_bounce:
    ldrh r6, [r_state, #OFFSET_DE_VOL]
    neg r6, r6
    strh r6, [r_state, #OFFSET_DE_VOL]
    add r5, r6
    bx lr


.align 2
channel_logic:
    .byte 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1

    // dword mask = (1 << r.env);
    // if (mask &
    //     ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) |
    //      (1 << 15)))
    //     env = denv = 0;
    // else if (mask & ((1 << 8) | (1 << 12)))
    //     env &= 31;
    // else if (mask & ((1 << 10) | (1 << 14)))
    //     denv = -denv, env = env + denv;
    // else env = 31, denv = 0; //11,13
.global envelope_switch
envelope_switch:
    .word envelope_floor  // 0
    .word envelope_floor  // 1
    .word envelope_floor  // 2
    .word envelope_floor  // 3
    .word envelope_floor  // 4
    .word envelope_floor  // 5
    .word envelope_floor  // 6
    .word envelope_floor  // 7
    .word envelope_wrap   // 8
    .word envelope_floor  // 9
    .word envelope_bounce // 10
    .word envelope_ceil   // 11
    .word envelope_wrap   // 12
    .word envelope_ceil   // 13
    .word envelope_bounce // 14
    .word envelope_floor  // 15

.global voltab_ay
voltab_ay:

.hword 0x0000,0x0000,0x0340,0x0340,0x04C0,0x04C0,0x06F2,0x06F2,0x0A44,0x0A44,0x0F13,0x0F13,0x1510,0x1510,0x227E,0x227E
.hword 0x289F,0x289F,0x414E,0x414E,0x5B21,0x5B21,0x7258,0x7258,0x905E,0x905E,0xB550,0xB550,0xD7A0,0xD7A0,0xFFFF,0xFFFF

.global voltab_ym
voltab_ym:
.hword 0x0000,0x0000,0x00EF,0x01D0,0x0290,0x032A,0x03EE,0x04D2,0x0611,0x0782,0x0912,0x0A36,0x0C31,0x0EB6,0x1130,0x13A0
.hword 0x1751,0x1BF5,0x20E2,0x2594,0x2CA1,0x357F,0x3E45,0x475E,0x5502,0x6620,0x7730,0x8844,0xA1D2,0xC102,0xE0A2,0xFFFF

.global envelope_switch

.section .data
.global current_voltab
current_voltab:
.word voltab_ay
.global envelope_handler
envelope_handler:
.word envelope_floor