/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SOFTWARE_Z80KHAN_H
#define SOFTWARE_Z80KHAN_H

#ifdef __cplusplus
extern "C" {
#endif
// reset the z80
extern void z80a_reset();
//// prepare for execution by loading execution state into registers
//extern void z80a_load_state();
//// save execution state back to memory
//extern void z80a_unload_state();

// this is very inefficient as it full restores then saves the processor state
extern void z80a_step();
// run until t >= FRAME_TACTS (or t >= 0 if USE_Z80_ARM_OFFSET_T)
extern void z80a_update(int int_len);
extern void z80a_breakpoint_hit();

#pragma pack(push, 1)

#ifdef USE_BIG_ENDIAN
#define DECLARE_ZREG16(reg, low, high)\
union\
{\
    struct\
    {\
        uint8_t reg##xx;\
        uint8_t reg##x;\
        uint8_t high;\
        uint8_t low;\
    };\
    int reg;\
};
#else//USE_BIG_ENDIAN
#define DECLARE_ZREG16(reg, low, high)\
union\
{\
    struct\
    {\
        uint8_t low;\
        uint8_t high;\
        uint8_t reg##x;\
        uint8_t reg##xx;\
    };\
    int reg;\
};
#endif//USE_BIG_ENDIAN

extern struct _z80a_caller_regs *z80a_caller_regs;
struct _z80a_resting_state {
// argghhhh had fancy __builtin_offsetof stuff here, but was bitten by the fact that i'm compiling for generation on 64 bit :-(
#define rs_offsetof_im 0
    uint32_t im;
#define rs_offsetof_eipos (rs_offsetof_im + 4)
    int32_t eipos;
    union {
        struct {
#define rs_offsetof_r_hi (rs_offsetof_eipos + 4)
            uint8_t r_hi;
#define rs_offsetof_iff1 (rs_offsetof_r_hi + 1)
            uint8_t iff1;
#define rs_offsetof_iff2 (rs_offsetof_iff1 + 1)
            uint8_t iff2;
#define rs_offsetof_halted (rs_offsetof_iff2 + 1)
            uint8_t halted;
        };
        uint32_t int_flags;
    };
    union {
        struct {
#define rs_offsetof_r_low (rs_offsetof_halted + 1)
            uint8_t r_low;
#define rs_offsetof_i (rs_offsetof_r_low + 1)
            uint8_t i;
        };
        uint32_t ir;
    };
    uint32_t ix;
    uint32_t iy;

    uint32_t pc;
#ifndef USE_BANKED_MEMORY_ACCESS
#ifdef USE_SINGLE_64K_MEMORY
    uint8_t *memory_64k;
#else
#if PICO_ON_DEVICE
#error require USE_BANKED_MEMORY_ACCESS for non 64K
#endif
#endif
#else
    void *interp;
#endif
    int32_t t;
    DECLARE_ZREG16(af, f, a)

    DECLARE_ZREG16(bc, c, b)
    DECLARE_ZREG16(de, e, d)
    DECLARE_ZREG16(hl, l, h)
    DECLARE_ZREG16(memptr, mem_l, mem_h)
    DECLARE_ZREG16(sp, sp_l, sp_h)
#define rs_offsetof_alt_af 60
    uint32_t alt_af;
#define rs_offsetof_alt_bc (rs_offsetof_alt_af + 4)
    uint32_t alt_bc;
#define rs_offsetof_alt_de (rs_offsetof_alt_bc + 4)
    uint32_t alt_de;
#define rs_offsetof_alt_hl (rs_offsetof_alt_de + 4)
    uint32_t alt_hl;
#define rs_offsetof_scratch (rs_offsetof_alt_hl + 4)
    uint32_t scratch;

    void (*step_hook_func)(); // called for each instruction
    uint32_t active; // not byte for assembly offset distance reasons.
#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
    uint32_t bp_addr;
    uint32_t last_pc;
#endif
#endif
};
#pragma pack(pop)

extern _z80a_resting_state z80a_resting_state;
#ifdef __cplusplus
}
#endif

#endif //SOFTWARE_Z80KHAN_H
