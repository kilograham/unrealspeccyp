/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2010 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
Copyright (C) 2023 Graham Sanderson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../std.h"
#include "../devices/memory.h"
#include "../devices/ula.h"
#include "../devices/device.h"

#include "z80t.h"

namespace Z80t
{
    const char *eZ80t::arm_regs[REG_COUNT];

    static eZ80t::CALLFUNC seen_opcodes[2048];
    static const char *seen_prefix[2048];
    static int seen_num[2048];

    int seen_opcode_count;

    bool eZ80t::lr_saved;
    bool eZ80t::r_temp_used;
    bool eZ80t::r_temp_restricted_used;

    rs_var_init(halted);
    rs_var_init(iff1);
    rs_var_init(iff2);
    rs_var_init(r_hi);
    rs_var_init(r_low);
    rs_var_init(i);
    rs_var_init(im);
    rs_var_init(eipos);

    rs_var_struct_init(alt, bc);
    rs_var_struct_init(alt, de);
    rs_var_struct_init(alt, hl);
    rs_var_struct_init(alt, af);
    rs_var_init(scratch);

    const char *eZ80t::pending_call = NULL;

#define IMPL_REG16(REGNAME, reg, low, high)\
    struct eZ80t::Reg16<REGNAME> eZ80t::reg; \
    struct eZ80t::RegLo<REGNAME> eZ80t::low; \
    struct eZ80t::RegHi<REGNAME> eZ80t::high;

    IMPL_REG16(PC, pc, pc_l, pc_h)
    IMPL_REG16(SP, sp, sp_l, sp_h)
    IMPL_REG16(MEMPTR, memptr, mem_l, mem_h)	// undocumented register
    IMPL_REG16(IX, ix, xl, xh)
    IMPL_REG16(IY, iy, yl, yh)

    IMPL_REG16(BC, bc, c, b)
    IMPL_REG16(DE, de, e, d)
    IMPL_REG16(HL, hl, l, h)
    IMPL_REG16(AF, af, f, a)
    IMPL_REG16(TEMPORARY, temporary, temp_lo, temp_hi)
    IMPL_REG16(TEMPORARY_RESTRICTED, restricted, restriced_lo, restricted_hi)

    //=============================================================================
    //	eZ80t::eZ80
    //-----------------------------------------------------------------------------
    eZ80t::eZ80t()
    {

        arm_regs[BC] = "r_bc";
        arm_regs[DE] = "r_de";
        arm_regs[AF] = "r_af";
        arm_regs[IX] = "r_ixy";
        arm_regs[IY] = "r_ixy";
        arm_regs[MEMPTR] = "r_memptr";
        arm_regs[PC] = "r_pc";
        arm_regs[HL] = "r_hl";
        arm_regs[SP] = "r_sp";
        arm_regs[TEMPORARY] = "r_temp";
        arm_regs[TEMPORARY_RESTRICTED] = "r_temp_restricted";
    }
    //=============================================================================
    eZ80t::ZeroExtendedByteInR0 eZ80t::IoRead(WordInR0 port) const {
        // address already in r0
        emit_call_func("ioread8");
        return ZeroExtendedByteInR0();
    }
    void eZ80t::IoWrite(WordInR1 port, LoByteAndGarbageInR0 v) {
        emit_call_func("iowrite8");
    }
    eZ80t::ZeroExtendedByteInR0 eZ80t::Read(WordInR0 addr) const {
        // address already in r0
#ifndef USE_SINGLE_64K_MEMORY
        emit_call_func("read8");
#else
        emit("read8_internal", "r0");
#endif
        return ZeroExtendedByteInR0();
    }
    eZ80t::ZeroExtendedByteInR0 eZ80t::ReadInc(Reg16Ref addr) const {
        emit_r0_from_reg(addr);
        emit_call_func("read8inc");
        emit_reg_from_r1(addr);
        return ZeroExtendedByteInR0();
    }
    eZ80t::WordInR0 eZ80t::Read2(WordInR0 addr) const {
        // address already in r0
        emit_call_func("read16");
        return WordInR0();
    } // low then high
    eZ80t::WordInR0 eZ80t::Read2Inc(Reg16Ref addr) const {
        emit_r0_from_reg(addr);
        emit_call_func("read16inc");
        emit_reg_from_r1(addr);
        return WordInR0();
    }
    void eZ80t::Write(Reg16Ref addr, LoByteAndGarbageInR0 v) {
        WordInR1 __unused x = addr;
        emit_call_func("write8");
    }
    void eZ80t::WriteXY(WordInR0 addr, Reg8Ref v) {
        emit("mov ", "r1, r0");
        if (v.lo) {
            emit_r0_from_reg_lo(v);
        } else {
            emit_r0_from_reg_hi(v);
        }
        emit_call_func("write8");
    }

    void eZ80t::Write2(Reg16Ref addr, WordInR0 v) {
        emit_r1_from_reg(addr);
        emit_call_func("write16");
    } // low then high

    void eZ80t::generate_arm() {
        emit("// ================================");
        emit("// == AUTOGENERATED: DO NOT EDIT ==");
        emit("// ================================");
        emit("");
        emit("// ALWAYS DIFF THIS CODE AFTER GENERATION - SENSITVE TO COMPILER VERSION CHANGES!");
        emit("");

        const CALLFUNC opcodes[] =
                {
                        &eZ80t::Op00, &eZ80t::Op01, &eZ80t::Op02, &eZ80t::Op03, &eZ80t::Op04, &eZ80t::Op05,
                        &eZ80t::Op06, &eZ80t::Op07,
                        &eZ80t::Op08, &eZ80t::Op09, &eZ80t::Op0A, &eZ80t::Op0B, &eZ80t::Op0C, &eZ80t::Op0D,
                        &eZ80t::Op0E, &eZ80t::Op0F,
                        &eZ80t::Op10, &eZ80t::Op11, &eZ80t::Op12, &eZ80t::Op13, &eZ80t::Op14, &eZ80t::Op15,
                        &eZ80t::Op16, &eZ80t::Op17,
                        &eZ80t::Op18, &eZ80t::Op19, &eZ80t::Op1A, &eZ80t::Op1B, &eZ80t::Op1C, &eZ80t::Op1D,
                        &eZ80t::Op1E, &eZ80t::Op1F,
                        &eZ80t::Op20, &eZ80t::Op21, &eZ80t::Op22, &eZ80t::Op23, &eZ80t::Op24, &eZ80t::Op25,
                        &eZ80t::Op26, &eZ80t::Op27,
                        &eZ80t::Op28, &eZ80t::Op29, &eZ80t::Op2A, &eZ80t::Op2B, &eZ80t::Op2C, &eZ80t::Op2D,
                        &eZ80t::Op2E, &eZ80t::Op2F,
                        &eZ80t::Op30, &eZ80t::Op31, &eZ80t::Op32, &eZ80t::Op33, &eZ80t::Op34, &eZ80t::Op35,
                        &eZ80t::Op36, &eZ80t::Op37,
                        &eZ80t::Op38, &eZ80t::Op39, &eZ80t::Op3A, &eZ80t::Op3B, &eZ80t::Op3C, &eZ80t::Op3D,
                        &eZ80t::Op3E, &eZ80t::Op3F,

                        &eZ80t::Op40, &eZ80t::Op41, &eZ80t::Op42, &eZ80t::Op43, &eZ80t::Op44, &eZ80t::Op45,
                        &eZ80t::Op46, &eZ80t::Op47,
                        &eZ80t::Op48, &eZ80t::Op49, &eZ80t::Op4A, &eZ80t::Op4B, &eZ80t::Op4C, &eZ80t::Op4D,
                        &eZ80t::Op4E, &eZ80t::Op4F,
                        &eZ80t::Op50, &eZ80t::Op51, &eZ80t::Op52, &eZ80t::Op53, &eZ80t::Op54, &eZ80t::Op55,
                        &eZ80t::Op56, &eZ80t::Op57,
                        &eZ80t::Op58, &eZ80t::Op59, &eZ80t::Op5A, &eZ80t::Op5B, &eZ80t::Op5C, &eZ80t::Op5D,
                        &eZ80t::Op5E, &eZ80t::Op5F,
                        &eZ80t::Op60, &eZ80t::Op61, &eZ80t::Op62, &eZ80t::Op63, &eZ80t::Op64, &eZ80t::Op65,
                        &eZ80t::Op66, &eZ80t::Op67,
                        &eZ80t::Op68, &eZ80t::Op69, &eZ80t::Op6A, &eZ80t::Op6B, &eZ80t::Op6C, &eZ80t::Op6D,
                        &eZ80t::Op6E, &eZ80t::Op6F,
                        &eZ80t::Op70, &eZ80t::Op71, &eZ80t::Op72, &eZ80t::Op73, &eZ80t::Op74, &eZ80t::Op75,
                        &eZ80t::Op76, &eZ80t::Op77,
                        &eZ80t::Op78, &eZ80t::Op79, &eZ80t::Op7A, &eZ80t::Op7B, &eZ80t::Op7C, &eZ80t::Op7D,
                        &eZ80t::Op7E, &eZ80t::Op7F,

                        &eZ80t::Op80, &eZ80t::Op81, &eZ80t::Op82, &eZ80t::Op83, &eZ80t::Op84, &eZ80t::Op85,
                        &eZ80t::Op86, &eZ80t::Op87,
                        &eZ80t::Op88, &eZ80t::Op89, &eZ80t::Op8A, &eZ80t::Op8B, &eZ80t::Op8C, &eZ80t::Op8D,
                        &eZ80t::Op8E, &eZ80t::Op8F,
                        &eZ80t::Op90, &eZ80t::Op91, &eZ80t::Op92, &eZ80t::Op93, &eZ80t::Op94, &eZ80t::Op95,
                        &eZ80t::Op96, &eZ80t::Op97,
                        &eZ80t::Op98, &eZ80t::Op99, &eZ80t::Op9A, &eZ80t::Op9B, &eZ80t::Op9C, &eZ80t::Op9D,
                        &eZ80t::Op9E, &eZ80t::Op9F,
                        &eZ80t::OpA0, &eZ80t::OpA1, &eZ80t::OpA2, &eZ80t::OpA3, &eZ80t::OpA4, &eZ80t::OpA5,
                        &eZ80t::OpA6, &eZ80t::OpA7,
                        &eZ80t::OpA8, &eZ80t::OpA9, &eZ80t::OpAA, &eZ80t::OpAB, &eZ80t::OpAC, &eZ80t::OpAD,
                        &eZ80t::OpAE, &eZ80t::OpAF,
                        &eZ80t::OpB0, &eZ80t::OpB1, &eZ80t::OpB2, &eZ80t::OpB3, &eZ80t::OpB4, &eZ80t::OpB5,
                        &eZ80t::OpB6, &eZ80t::OpB7,
                        &eZ80t::OpB8, &eZ80t::OpB9, &eZ80t::OpBA, &eZ80t::OpBB, &eZ80t::OpBC, &eZ80t::OpBD,
                        &eZ80t::OpBE, &eZ80t::OpBF,

                        &eZ80t::OpC0, &eZ80t::OpC1, &eZ80t::OpC2, &eZ80t::OpC3, &eZ80t::OpC4, &eZ80t::OpC5,
                        &eZ80t::OpC6, &eZ80t::OpC7,
                        &eZ80t::OpC8, &eZ80t::OpC9, &eZ80t::OpCA, &eZ80t::OpCB, &eZ80t::OpCC, &eZ80t::OpCD,
                        &eZ80t::OpCE, &eZ80t::OpCF,
                        &eZ80t::OpD0, &eZ80t::OpD1, &eZ80t::OpD2, &eZ80t::OpD3, &eZ80t::OpD4, &eZ80t::OpD5,
                        &eZ80t::OpD6, &eZ80t::OpD7,
                        &eZ80t::OpD8, &eZ80t::OpD9, &eZ80t::OpDA, &eZ80t::OpDB, &eZ80t::OpDC, &eZ80t::OpDD,
                        &eZ80t::OpDE, &eZ80t::OpDF,
                        &eZ80t::OpE0, &eZ80t::OpE1, &eZ80t::OpE2, &eZ80t::OpE3, &eZ80t::OpE4, &eZ80t::OpE5,
                        &eZ80t::OpE6, &eZ80t::OpE7,
                        &eZ80t::OpE8, &eZ80t::OpE9, &eZ80t::OpEA, &eZ80t::OpEB, &eZ80t::OpEC, &eZ80t::OpED,
                        &eZ80t::OpEE, &eZ80t::OpEF,
                        &eZ80t::OpF0, &eZ80t::OpF1, &eZ80t::OpF2, &eZ80t::OpF3, &eZ80t::OpF4, &eZ80t::OpF5,
                        &eZ80t::OpF6, &eZ80t::OpF7,
                        &eZ80t::OpF8, &eZ80t::OpF9, &eZ80t::OpFA, &eZ80t::OpFB, &eZ80t::OpFC, &eZ80t::OpFD,
                        &eZ80t::OpFE, &eZ80t::OpFF
                };
        dump_instructions("op", opcodes, count_of(opcodes), "top level opcodes");

        const CALLFUNC oplscodes[] = {
                &eZ80t::Opl_rlc,
                &eZ80t::Opl_rrc,
                &eZ80t::Opl_rl,
                &eZ80t::Opl_rr,
                &eZ80t::Opl_sla,
                &eZ80t::Opl_sra,
                &eZ80t::Opl_sli,
                &eZ80t::Opl_srl,
                &eZ80t::Opl_bit0,
                &eZ80t::Opl_bit1,
                &eZ80t::Opl_bit2,
                &eZ80t::Opl_bit3,
                &eZ80t::Opl_bit4,
                &eZ80t::Opl_bit5,
                &eZ80t::Opl_bit6,
                &eZ80t::Opl_bit7,
                &eZ80t::Opl_res0,
                &eZ80t::Opl_res1,
                &eZ80t::Opl_res2,
                &eZ80t::Opl_res3,
                &eZ80t::Opl_res4,
                &eZ80t::Opl_res5,
                &eZ80t::Opl_res6,
                &eZ80t::Opl_res7,
                &eZ80t::Opl_set0,
                &eZ80t::Opl_set1,
                &eZ80t::Opl_set2,
                &eZ80t::Opl_set3,
                &eZ80t::Opl_set4,
                &eZ80t::Opl_set5,
                &eZ80t::Opl_set6,
                &eZ80t::Opl_set7,
        };

        dump_instructions("opli_r35_", oplscodes, count_of(oplscodes), "OPL inner logic functions which do core work on r_temp", 8);

        const CALLFUNC oplsmcodes[] = {
                &eZ80t::Opl_rlc,
                &eZ80t::Opl_rrc,
                &eZ80t::Opl_rl,
                &eZ80t::Opl_rr,
                &eZ80t::Opl_sla,
                &eZ80t::Opl_sra,
                &eZ80t::Opl_sli,
                &eZ80t::Opl_srl,
                &eZ80t::Opl_bit0m,
                &eZ80t::Opl_bit1m,
                &eZ80t::Opl_bit2m,
                &eZ80t::Opl_bit3m,
                &eZ80t::Opl_bit4m,
                &eZ80t::Opl_bit5m,
                &eZ80t::Opl_bit6m,
                &eZ80t::Opl_bit7m,
                &eZ80t::Opl_res0,
                &eZ80t::Opl_res1,
                &eZ80t::Opl_res2,
                &eZ80t::Opl_res3,
                &eZ80t::Opl_res4,
                &eZ80t::Opl_res5,
                &eZ80t::Opl_res6,
                &eZ80t::Opl_res7,
                &eZ80t::Opl_set0,
                &eZ80t::Opl_set1,
                &eZ80t::Opl_set2,
                &eZ80t::Opl_set3,
                &eZ80t::Opl_set4,
                &eZ80t::Opl_set5,
                &eZ80t::Opl_set6,
                &eZ80t::Opl_set7,
        };
        dump_instructions("opli_m35_", oplsmcodes, count_of(oplsmcodes), "OPL inner logic functions which do core work on r_temp, but set F3&F5 based on mem_h", 8);

#ifdef USE_LARGER_FASTER_CB
        const CALLFUNC oplcodes[] =
            {
                &eZ80t::Opl00, &eZ80t::Opl01, &eZ80t::Opl02, &eZ80t::Opl03, &eZ80t::Opl04, &eZ80t::Opl05, &eZ80t::Opl06, &eZ80t::Opl07,
                &eZ80t::Opl08, &eZ80t::Opl09, &eZ80t::Opl0A, &eZ80t::Opl0B, &eZ80t::Opl0C, &eZ80t::Opl0D, &eZ80t::Opl0E, &eZ80t::Opl0F,
                &eZ80t::Opl10, &eZ80t::Opl11, &eZ80t::Opl12, &eZ80t::Opl13, &eZ80t::Opl14, &eZ80t::Opl15, &eZ80t::Opl16, &eZ80t::Opl17,
                &eZ80t::Opl18, &eZ80t::Opl19, &eZ80t::Opl1A, &eZ80t::Opl1B, &eZ80t::Opl1C, &eZ80t::Opl1D, &eZ80t::Opl1E, &eZ80t::Opl1F,
                &eZ80t::Opl20, &eZ80t::Opl21, &eZ80t::Opl22, &eZ80t::Opl23, &eZ80t::Opl24, &eZ80t::Opl25, &eZ80t::Opl26, &eZ80t::Opl27,
                &eZ80t::Opl28, &eZ80t::Opl29, &eZ80t::Opl2A, &eZ80t::Opl2B, &eZ80t::Opl2C, &eZ80t::Opl2D, &eZ80t::Opl2E, &eZ80t::Opl2F,
                &eZ80t::Opl30, &eZ80t::Opl31, &eZ80t::Opl32, &eZ80t::Opl33, &eZ80t::Opl34, &eZ80t::Opl35, &eZ80t::Opl36, &eZ80t::Opl37,
                &eZ80t::Opl38, &eZ80t::Opl39, &eZ80t::Opl3A, &eZ80t::Opl3B, &eZ80t::Opl3C, &eZ80t::Opl3D, &eZ80t::Opl3E, &eZ80t::Opl3F,

                &eZ80t::Opl40, &eZ80t::Opl41, &eZ80t::Opl42, &eZ80t::Opl43, &eZ80t::Opl44, &eZ80t::Opl45, &eZ80t::Opl46, &eZ80t::Opl47,
                &eZ80t::Opl48, &eZ80t::Opl49, &eZ80t::Opl4A, &eZ80t::Opl4B, &eZ80t::Opl4C, &eZ80t::Opl4D, &eZ80t::Opl4E, &eZ80t::Opl4F,
                &eZ80t::Opl50, &eZ80t::Opl51, &eZ80t::Opl52, &eZ80t::Opl53, &eZ80t::Opl54, &eZ80t::Opl55, &eZ80t::Opl56, &eZ80t::Opl57,
                &eZ80t::Opl58, &eZ80t::Opl59, &eZ80t::Opl5A, &eZ80t::Opl5B, &eZ80t::Opl5C, &eZ80t::Opl5D, &eZ80t::Opl5E, &eZ80t::Opl5F,
                &eZ80t::Opl60, &eZ80t::Opl61, &eZ80t::Opl62, &eZ80t::Opl63, &eZ80t::Opl64, &eZ80t::Opl65, &eZ80t::Opl66, &eZ80t::Opl67,
                &eZ80t::Opl68, &eZ80t::Opl69, &eZ80t::Opl6A, &eZ80t::Opl6B, &eZ80t::Opl6C, &eZ80t::Opl6D, &eZ80t::Opl6E, &eZ80t::Opl6F,
                &eZ80t::Opl70, &eZ80t::Opl71, &eZ80t::Opl72, &eZ80t::Opl73, &eZ80t::Opl74, &eZ80t::Opl75, &eZ80t::Opl76, &eZ80t::Opl77,
                &eZ80t::Opl78, &eZ80t::Opl79, &eZ80t::Opl7A, &eZ80t::Opl7B, &eZ80t::Opl7C, &eZ80t::Opl7D, &eZ80t::Opl7E, &eZ80t::Opl7F,

                &eZ80t::Opl80, &eZ80t::Opl81, &eZ80t::Opl82, &eZ80t::Opl83, &eZ80t::Opl84, &eZ80t::Opl85, &eZ80t::Opl86, &eZ80t::Opl87,
                &eZ80t::Opl88, &eZ80t::Opl89, &eZ80t::Opl8A, &eZ80t::Opl8B, &eZ80t::Opl8C, &eZ80t::Opl8D, &eZ80t::Opl8E, &eZ80t::Opl8F,
                &eZ80t::Opl90, &eZ80t::Opl91, &eZ80t::Opl92, &eZ80t::Opl93, &eZ80t::Opl94, &eZ80t::Opl95, &eZ80t::Opl96, &eZ80t::Opl97,
                &eZ80t::Opl98, &eZ80t::Opl99, &eZ80t::Opl9A, &eZ80t::Opl9B, &eZ80t::Opl9C, &eZ80t::Opl9D, &eZ80t::Opl9E, &eZ80t::Opl9F,
                &eZ80t::OplA0, &eZ80t::OplA1, &eZ80t::OplA2, &eZ80t::OplA3, &eZ80t::OplA4, &eZ80t::OplA5, &eZ80t::OplA6, &eZ80t::OplA7,
                &eZ80t::OplA8, &eZ80t::OplA9, &eZ80t::OplAA, &eZ80t::OplAB, &eZ80t::OplAC, &eZ80t::OplAD, &eZ80t::OplAE, &eZ80t::OplAF,
                &eZ80t::OplB0, &eZ80t::OplB1, &eZ80t::OplB2, &eZ80t::OplB3, &eZ80t::OplB4, &eZ80t::OplB5, &eZ80t::OplB6, &eZ80t::OplB7,
                &eZ80t::OplB8, &eZ80t::OplB9, &eZ80t::OplBA, &eZ80t::OplBB, &eZ80t::OplBC, &eZ80t::OplBD, &eZ80t::OplBE, &eZ80t::OplBF,

                &eZ80t::OplC0, &eZ80t::OplC1, &eZ80t::OplC2, &eZ80t::OplC3, &eZ80t::OplC4, &eZ80t::OplC5, &eZ80t::OplC6, &eZ80t::OplC7,
                &eZ80t::OplC8, &eZ80t::OplC9, &eZ80t::OplCA, &eZ80t::OplCB, &eZ80t::OplCC, &eZ80t::OplCD, &eZ80t::OplCE, &eZ80t::OplCF,
                &eZ80t::OplD0, &eZ80t::OplD1, &eZ80t::OplD2, &eZ80t::OplD3, &eZ80t::OplD4, &eZ80t::OplD5, &eZ80t::OplD6, &eZ80t::OplD7,
                &eZ80t::OplD8, &eZ80t::OplD9, &eZ80t::OplDA, &eZ80t::OplDB, &eZ80t::OplDC, &eZ80t::OplDD, &eZ80t::OplDE, &eZ80t::OplDF,
                &eZ80t::OplE0, &eZ80t::OplE1, &eZ80t::OplE2, &eZ80t::OplE3, &eZ80t::OplE4, &eZ80t::OplE5, &eZ80t::OplE6, &eZ80t::OplE7,
                &eZ80t::OplE8, &eZ80t::OplE9, &eZ80t::OplEA, &eZ80t::OplEB, &eZ80t::OplEC, &eZ80t::OplED, &eZ80t::OplEE, &eZ80t::OplEF,
                &eZ80t::OplF0, &eZ80t::OplF1, &eZ80t::OplF2, &eZ80t::OplF3, &eZ80t::OplF4, &eZ80t::OplF5, &eZ80t::OplF6, &eZ80t::OplF7,
                &eZ80t::OplF8, &eZ80t::OplF9, &eZ80t::OplFA, &eZ80t::OplFB, &eZ80t::OplFC, &eZ80t::OplFD, &eZ80t::OplFE, &eZ80t::OplFF
            };

        dump_instructions("opl", oplcodes, count_of(oplcodes), "cb logic opcodes");
#endif

        CALLFUNC const opddcodes[] =
                {
                        &eZ80t::Op00, &eZ80t::Op01, &eZ80t::Op02, &eZ80t::Op03, &eZ80t::Op04, &eZ80t::Op05,
                        &eZ80t::Op06, &eZ80t::Op07,
                        &eZ80t::Op08, &eZ80t::Opx09, &eZ80t::Op0A, &eZ80t::Op0B, &eZ80t::Op0C, &eZ80t::Op0D,
                        &eZ80t::Op0E, &eZ80t::Op0F,
                        &eZ80t::Op10, &eZ80t::Op11, &eZ80t::Op12, &eZ80t::Op13, &eZ80t::Op14, &eZ80t::Op15,
                        &eZ80t::Op16, &eZ80t::Op17,
                        &eZ80t::Op18, &eZ80t::Opx19, &eZ80t::Op1A, &eZ80t::Op1B, &eZ80t::Op1C, &eZ80t::Op1D,
                        &eZ80t::Op1E, &eZ80t::Op1F,
                        &eZ80t::Op20, &eZ80t::Opx21, &eZ80t::Opx22, &eZ80t::Opx23, &eZ80t::Opx24, &eZ80t::Opx25,
                        &eZ80t::Opx26, &eZ80t::Op27,
                        &eZ80t::Op28, &eZ80t::Opx29, &eZ80t::Opx2A, &eZ80t::Opx2B, &eZ80t::Opx2C, &eZ80t::Opx2D,
                        &eZ80t::Opx2E, &eZ80t::Op2F,
                        &eZ80t::Op30, &eZ80t::Op31, &eZ80t::Op32, &eZ80t::Op33, &eZ80t::Opx34, &eZ80t::Opx35,
                        &eZ80t::Opx36, &eZ80t::Op37,
                        &eZ80t::Op38, &eZ80t::Opx39, &eZ80t::Op3A, &eZ80t::Op3B, &eZ80t::Op3C, &eZ80t::Op3D,
                        &eZ80t::Op3E, &eZ80t::Op3F,

                        &eZ80t::Op40, &eZ80t::Op41, &eZ80t::Op42, &eZ80t::Op43, &eZ80t::Opx44, &eZ80t::Opx45,
                        &eZ80t::Opx46, &eZ80t::Op47,
                        &eZ80t::Op48, &eZ80t::Op49, &eZ80t::Op4A, &eZ80t::Op4B, &eZ80t::Opx4C, &eZ80t::Opx4D,
                        &eZ80t::Opx4E, &eZ80t::Op4F,
                        &eZ80t::Op50, &eZ80t::Op51, &eZ80t::Op52, &eZ80t::Op53, &eZ80t::Opx54, &eZ80t::Opx55,
                        &eZ80t::Opx56, &eZ80t::Op57,
                        &eZ80t::Op58, &eZ80t::Op59, &eZ80t::Op5A, &eZ80t::Op5B, &eZ80t::Opx5C, &eZ80t::Opx5D,
                        &eZ80t::Opx5E, &eZ80t::Op5F,
                        &eZ80t::Opx60, &eZ80t::Opx61, &eZ80t::Opx62, &eZ80t::Opx63, &eZ80t::Op64, &eZ80t::Opx65,
                        &eZ80t::Opx66, &eZ80t::Opx67,
                        &eZ80t::Opx68, &eZ80t::Opx69, &eZ80t::Opx6A, &eZ80t::Opx6B, &eZ80t::Opx6C, &eZ80t::Op6D,
                        &eZ80t::Opx6E, &eZ80t::Opx6F,
                        &eZ80t::Opx70, &eZ80t::Opx71, &eZ80t::Opx72, &eZ80t::Opx73, &eZ80t::Opx74, &eZ80t::Opx75,
                        &eZ80t::Op76, &eZ80t::Opx77,
                        &eZ80t::Op78, &eZ80t::Op79, &eZ80t::Op7A, &eZ80t::Op7B, &eZ80t::Opx7C, &eZ80t::Opx7D,
                        &eZ80t::Opx7E, &eZ80t::Op7F,

                        &eZ80t::Op80, &eZ80t::Op81, &eZ80t::Op82, &eZ80t::Op83, &eZ80t::Opx84, &eZ80t::Opx85,
                        &eZ80t::Opx86, &eZ80t::Op87,
                        &eZ80t::Op88, &eZ80t::Op89, &eZ80t::Op8A, &eZ80t::Op8B, &eZ80t::Opx8C, &eZ80t::Opx8D,
                        &eZ80t::Opx8E, &eZ80t::Op8F,
                        &eZ80t::Op90, &eZ80t::Op91, &eZ80t::Op92, &eZ80t::Op93, &eZ80t::Opx94, &eZ80t::Opx95,
                        &eZ80t::Opx96, &eZ80t::Op97,
                        &eZ80t::Op98, &eZ80t::Op99, &eZ80t::Op9A, &eZ80t::Op9B, &eZ80t::Opx9C, &eZ80t::Opx9D,
                        &eZ80t::Opx9E, &eZ80t::Op9F,
                        &eZ80t::OpA0, &eZ80t::OpA1, &eZ80t::OpA2, &eZ80t::OpA3, &eZ80t::OpxA4, &eZ80t::OpxA5,
                        &eZ80t::OpxA6, &eZ80t::OpA7,
                        &eZ80t::OpA8, &eZ80t::OpA9, &eZ80t::OpAA, &eZ80t::OpAB, &eZ80t::OpxAC, &eZ80t::OpxAD,
                        &eZ80t::OpxAE, &eZ80t::OpAF,
                        &eZ80t::OpB0, &eZ80t::OpB1, &eZ80t::OpB2, &eZ80t::OpB3, &eZ80t::OpxB4, &eZ80t::OpxB5,
                        &eZ80t::OpxB6, &eZ80t::OpB7,
                        &eZ80t::OpB8, &eZ80t::OpB9, &eZ80t::OpBA, &eZ80t::OpBB, &eZ80t::OpxBC, &eZ80t::OpxBD,
                        &eZ80t::OpxBE, &eZ80t::OpBF,

                        &eZ80t::OpC0, &eZ80t::OpC1, &eZ80t::OpC2, &eZ80t::OpC3, &eZ80t::OpC4, &eZ80t::OpC5,
                        &eZ80t::OpC6, &eZ80t::OpC7,
                        &eZ80t::OpC8, &eZ80t::OpC9, &eZ80t::OpCA, &eZ80t::OpCB, &eZ80t::OpCC, &eZ80t::OpCD,
                        &eZ80t::OpCE, &eZ80t::OpCF,
                        &eZ80t::OpD0, &eZ80t::OpD1, &eZ80t::OpD2, &eZ80t::OpD3, &eZ80t::OpD4, &eZ80t::OpD5,
                        &eZ80t::OpD6, &eZ80t::OpD7,
                        &eZ80t::OpD8, &eZ80t::OpD9, &eZ80t::OpDA, &eZ80t::OpDB, &eZ80t::OpDC, &eZ80t::OpDD,
                        &eZ80t::OpDE, &eZ80t::OpDF,
                        &eZ80t::OpE0, &eZ80t::OpxE1, &eZ80t::OpE2, &eZ80t::OpxE3, &eZ80t::OpE4, &eZ80t::OpxE5,
                        &eZ80t::OpE6, &eZ80t::OpE7,
                        &eZ80t::OpE8, &eZ80t::OpxE9, &eZ80t::OpEA, &eZ80t::OpEB, &eZ80t::OpEC, &eZ80t::OpED,
                        &eZ80t::OpEE, &eZ80t::OpEF,
                        &eZ80t::OpF0, &eZ80t::OpF1, &eZ80t::OpF2, &eZ80t::OpF3, &eZ80t::OpF4, &eZ80t::OpF5,
                        &eZ80t::OpF6, &eZ80t::OpF7,
                        &eZ80t::OpF8, &eZ80t::OpxF9, &eZ80t::OpFA, &eZ80t::OpFB, &eZ80t::OpFC, &eZ80t::OpFD,
                        &eZ80t::OpFE, &eZ80t::OpFF
                };

        dump_instructions("opxy", opddcodes, count_of(opddcodes), "dd/fd prefix xy opcodes r_temp holds ix or iy");

        CALLFUNC const opedcodes[] =
                {
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,

                        &eZ80t::Ope40, &eZ80t::Ope41, &eZ80t::Ope42, &eZ80t::Ope43, &eZ80t::Ope44, &eZ80t::Ope45,
                        &eZ80t::Ope46, &eZ80t::Ope47,
                        &eZ80t::Ope48, &eZ80t::Ope49, &eZ80t::Ope4A, &eZ80t::Ope4B, &eZ80t::Ope4C, &eZ80t::Ope4D,
                        &eZ80t::Ope4E, &eZ80t::Ope4F,
                        &eZ80t::Ope50, &eZ80t::Ope51, &eZ80t::Ope52, &eZ80t::Ope53, &eZ80t::Ope54, &eZ80t::Ope55,
                        &eZ80t::Ope56, &eZ80t::Ope57,
                        &eZ80t::Ope58, &eZ80t::Ope59, &eZ80t::Ope5A, &eZ80t::Ope5B, &eZ80t::Ope5C, &eZ80t::Ope5D,
                        &eZ80t::Ope5E, &eZ80t::Ope5F,
                        &eZ80t::Ope60, &eZ80t::Ope61, &eZ80t::Ope62, &eZ80t::Ope63, &eZ80t::Ope64, &eZ80t::Ope65,
                        &eZ80t::Ope66, &eZ80t::Ope67,
                        &eZ80t::Ope68, &eZ80t::Ope69, &eZ80t::Ope6A, &eZ80t::Ope6B, &eZ80t::Ope6C, &eZ80t::Ope6D,
                        &eZ80t::Ope6E, &eZ80t::Ope6F,
                        &eZ80t::Ope70, &eZ80t::Ope71, &eZ80t::Ope72, &eZ80t::Ope73, &eZ80t::Ope74, &eZ80t::Ope75,
                        &eZ80t::Ope76, &eZ80t::Ope77,
                        &eZ80t::Ope78, &eZ80t::Ope79, &eZ80t::Ope7A, &eZ80t::Ope7B, &eZ80t::Ope7C, &eZ80t::Ope7D,
                        &eZ80t::Ope7E, &eZ80t::Ope7F,

                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::OpeA0, &eZ80t::OpeA1, &eZ80t::OpeA2, &eZ80t::OpeA3, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::OpeA8, &eZ80t::OpeA9, &eZ80t::OpeAA, &eZ80t::OpeAB, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::OpeB0, &eZ80t::OpeB1, &eZ80t::OpeB2, &eZ80t::OpeB3, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::OpeB8, &eZ80t::OpeB9, &eZ80t::OpeBA, &eZ80t::OpeBB, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,

                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00, &eZ80t::Op00,
                        &eZ80t::Op00, &eZ80t::Op00
                };


        dump_instructions("ope", opedcodes, count_of(opedcodes), "ed prefix");
    }

    void eZ80t::dump_instructions(const char *prefix, const CALLFUNC *instrs, int count, const char *description, int mult) {
        char buf[256];
        if (description) {
            snprintf(buf, sizeof(buf), "// === BEGIN %s", description);
            emit(buf);
        }
        int seen[count];
        for(int i=0;i<count;i++) {
            seen[i] = -1;
            for(int j=0;j<seen_opcode_count;j++) {
                if (seen_opcodes[j] == instrs[i]) {
                    seen[i] = j;
                    break;
                }
            }
        }
        // NOTE; we output the optable first (before the operations) because the 16 bit
        // offset will be zero extended, so we need it to be positive! (note we do
        // support negative for all but the main optable, since values from the main optable
        // may be used here
        // todo make more sense to put the subsequent tables b4 the main one and save a sxth

        // NOTE ALSO +1 for thumb mode

        // opbase must be the same as the first op_table, so we can just add the value
        // in the table to it to get the function address
        char table_name[128];
        if (strlen(prefix) && prefix[strlen(prefix)-1] == '_') {
            snprintf(table_name, sizeof(table_name), "%stable", prefix);
        } else {
            snprintf(table_name, sizeof(table_name), "%s_table", prefix);
        }
        snprintf(buf, sizeof(buf), "%s:", table_name);
        emit(buf);
        for(int i=0;i<count;i++) {
            if (seen[i] >= 0)
            {
               snprintf(buf, sizeof(buf), ".short %s%02x + 1 - %s", seen_prefix[seen[i]], seen_num[seen[i]], table_name);
            } else {
                snprintf(buf, sizeof(buf), ".short %s%02x + 1 - %s", prefix, i * mult, table_name);
            }
            emit(buf);
        }
        emit_helper_functions(prefix, HELPER_START);
        for(int i=0;i<count;i++) {
            if (seen[i] < 0)
            {
                assert(seen_opcode_count != sizeof(seen_opcodes));
                seen_prefix[seen_opcode_count] = prefix;
                seen_num[seen_opcode_count] = i * mult;
                seen_opcodes[seen_opcode_count++] = instrs[i];
                emit_helper_functions(prefix, i * mult);
                reset_function_state();
                if (!strcmp(prefix, "opl_inner_regular_35"))
                {
                    emit(".thumb_func");
                }
                snprintf(buf, sizeof(buf), "%s%02x:", prefix, i * mult);
                emit(buf);
                (this->*instrs[i])();
                emit_function_return();
                if (!strcmp(prefix, "opxy")) {
                    // can't use r_temp and r_ixy since they are the same reg
                    assert(!r_temp_used);
                }
            }
            if (i && !(i&31)) {
                // code a bit too big to not have frequent references
                emit(".ltorg");
            }
            if (i == 0xb0 && !strcmp(prefix, "ope")) {
                // extra one
                emit(".ltorg");
            }
        }
        emit_helper_functions(prefix, HELPER_END);
        emit(".ltorg");
        if (description) {
            snprintf(buf, sizeof(buf), "// === END %s", description);
            emit(buf);
        }
        printf("\n");
    }

}//namespace xZ80
