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

#ifndef	__Z80X_H__
#define	__Z80X_H__

#include "pico.h"
#include "../std_types.h"
#include <cstdio>
#include <cstring>
#include "z80khan.h" // needed for struct layout
#include <type_traits>

#ifdef DECLARE_REG16
#error z80 already included in same compilation unit
#endif

#pragma once

class eMemory;
class eRom;
class eUla;
class eDevices;

extern bool always_true;
#define USE_Z80T
namespace Z80t
{

#define DECLARE_REG16(REGNAME, reg, low, high)\
    static struct Reg16<REGNAME> reg; \
    static struct RegLo<REGNAME> low; \
    static struct RegHi<REGNAME> high;

    enum eFlags
    {
        CF = 0x01,
        NF = 0x02,
        PV = 0x04,
        F3 = 0x08,
        HF = 0x10,
        F5 = 0x20,
        ZF = 0x40,
        SF = 0x80
    };

    enum Reg16Name {
        BC,
        DE,
        HL,
        AF,
        IX,
        IY,
        MEMPTR,
        PC,
        SP,
        TEMPORARY,
        TEMPORARY_RESTRICTED,
        REG_COUNT
    };

#define R_TEMP_ARM 3

#define unimpl_assert(x) emit("bkpt", "#0", "// not implemented")

//*****************************************************************************
//	eZ80
//-----------------------------------------------------------------------------
class eZ80t
{
public:

    // is the register stored in an ARM low register (i.e. we can do direct arithmetic on it)
    static bool is_arm_lo_reg(const enum Reg16Name x) {
        return x == TEMPORARY || x == PC || x == AF || x == TEMPORARY_RESTRICTED;
    }

    template <enum Reg16Name R> struct RegLo;
    template <enum Reg16Name R> struct RegHi;

    // reference to an 8 bit or 16 bit reg
    struct RegRef {
        RegRef(const enum Reg16Name& reg) : reg(reg) {
            if (reg == TEMPORARY) {
                r_temp_used = true;
            }
            if (reg == TEMPORARY_RESTRICTED) {
                r_temp_restricted_used = true;
            }
        }
        enum Reg16Name reg;
    };

    struct Reg8Ref : public RegRef {
        Reg8Ref(const enum Reg16Name& reg, bool lo) : RegRef(reg), lo(lo) {}
        bool lo;
    };

    struct Reg16Ref : public RegRef
    {
        Reg16Ref(const enum Reg16Name &reg) : RegRef(reg) {}
    };

    struct WordInR0HighWordUnknown;
    struct WordInR1;
    struct ZeroExtendedByteInR0;

    // this is a value in r0 that is explicitly constructed from either a value already in r0 or a Reg16Ref
    struct WordInR0 {
        WordInR0() {
            // already in r0
        }
        WordInR0(const WordInR0HighWordUnknown& x) {
            emit("uxth", "r0, r0");
        }
        WordInR0(const Reg16Ref& ref) {
            char buf[128];
            // assign to r0
            snprintf(buf, sizeof(buf), "r0, %s", arm_regs[ref.reg]);
            emit("mov ", buf);
        }
        WordInR0 operator|(const WordInR1& v) {
            emit("orrs ", "r0, r1");
            return {};
        }
        WordInR0HighWordUnknown operator+(int x)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "r0, #%d", x);
            emit("adds ", buf);
            return {};
        }
    };

    // this is a value in r1 that is explicitly constructed from either a value already in r1 or a Reg16Ref
    struct WordInR1 {
        WordInR1() {
            // already in r1
        }
        WordInR1(const Reg16Ref& ref) {
            emit_r1_from_reg(ref);
        }
//        WordInR0 operator|(const ZeroExtendedByteInR0& v) {
//            emit("orrs ", "r0, r1");
//            return {};
//        }
    };

    // this is a value in r0 that is explicitly constructed form a value in r0 which may need zeroing
    struct WordInR0HighWordUnknown {
        WordInR0 operator&(int v) {
            assert(v == 255);
            emit("uxtb", "r0, r0");
            return {};
        }
    };

    // this is a value in r0 that is explicitly constructed form either a value in r0, a Reg8Ref or a constant
    struct ZeroExtendedByteInR0 {
        ZeroExtendedByteInR0() {
            // value already in r0
        }
        ZeroExtendedByteInR0(int val) {
            char buf[32];
            snprintf(buf, sizeof(buf), "r0, #%d", val);
            emit("movs ", buf);
        }
        ZeroExtendedByteInR0(const Reg8Ref& ref) {
            char buf[128];
            // assign to r0
            if (ref.lo) {
                emit_zero_extend_z80_lo_to_arm(ref, 0);
            } else {
                if (is_arm_lo_reg(ref.reg)) {
                    snprintf(buf, sizeof(buf), "r0, %s, #8", arm_regs[ref.reg]);
                    emit("lsrs ", buf);
                } else {
                    snprintf(buf, sizeof(buf), "r0, %s", arm_regs[ref.reg]);
                    emit("mov ", buf);
                    emit("lsrs ", "r0, #8");
                }
            }
        }

        operator WordInR0() {
            return {};
        }
    };

    // similar to ZeroExtendedByteInR0 except the hi byte may not be zero.
    struct LoByteAndGarbageInR0 {
        // value already in r0
        LoByteAndGarbageInR0() {}

        LoByteAndGarbageInR0(const ZeroExtendedByteInR0& r) {}

        LoByteAndGarbageInR0(int val) {
            char buf[32];
            snprintf(buf, sizeof(buf), "r0, #%d", val);
            emit("movs ", buf);
        }
        LoByteAndGarbageInR0(const Reg8Ref& ref) {
            char buf[128];
            // assign to r0
            if (ref.lo) {
                snprintf(buf, sizeof(buf), "r0, %s", arm_regs[ref.reg]);
                emit("mov ", buf, "// high half of word is ignored later");
            } else {
                snprintf(buf, sizeof(buf), "r0, %s", arm_regs[ref.reg]);
                emit("mov ", buf);
                emit("lsrs ", "r0, #8");
            }
        }
    };

    // note high word may not be corrsect, but we don't care
    struct SignExtendedByteInR0HighWordUnknown {
        SignExtendedByteInR0HighWordUnknown(ZeroExtendedByteInR0 v) {
            emit("sxtb", "r0, r0");
        }

        WordInR0HighWordUnknown operator+(int val) {
            assert(val == 1);
            emit("adds ", "r0, #1");
            return {};
        }

        operator WordInR0HighWordUnknown() {
            return {};
        }
    };

    // type of t variable
    struct TState {
        TState() {}
        TState operator+=(int val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "r_t, #%d", val);
            emit("adds ", buf);
            return *this;
        }
        TState operator++(int) {
            TState tmp = *this;
            emit("adds ", "r_t, #1");
            return tmp;
        }
    };

    // variable that doesn't have a dedicated register, so is load and stored to/from the resting state
    struct RestingStateVariable {
    private:
    public:
        RestingStateVariable(const char *name, int size, int offset) : name(name), offset(offset), size(size) {}
        RestingStateVariable(const RestingStateVariable& v) = delete;
        WordInR0 operator=(const RestingStateVariable&v) {
            WordInR0 tmp = v;
            return *this = tmp;
        }

        void emit_read(int target, int state_ref) const {
            char buf[32];
            snprintf(buf, sizeof(buf), "r%d, [r%d, #%d] // %s", target, state_ref, offset, name);
            switch (size) {
                case 1:
                    emit("ldrb", buf);
                    break;
                case 2:
                    emit("ldrh", buf);
                    break;
                case 4:
                    emit("ldr ", buf);
                    break;
                default:
                    assert(false);
            }
        }

        void emit_read_to_r1() const {
            emit("ldr ", "r1, =z80a_resting_state");
            emit_read(1, 1);
        }

        void emit_read_to_r0() const {
            emit("ldr ", "r0, =z80a_resting_state");
            emit_read(0, 0);
        }

        void emit_write(int source, int state_ref) {
            char buf[32];
            snprintf(buf, sizeof(buf), "r%d, [r%d, #%d] // %s", source, state_ref, offset, name);
            switch (size) {
                case 1:
                    emit("strb", buf);
                    break;
                case 2:
                    emit("strh", buf);
                    break;
                case 4:
                    emit("str ", buf);
                    break;
                default:
                    assert(false);
            }
        }

        void emit_write_from_r0() {
            emit("ldr ", "r1, =z80a_resting_state");
            emit_write(0, 1);
        }

        WordInR0 operator=(int val) {
            char buf[32];
            snprintf(buf, sizeof(buf), "r0, #%d", val);
            emit("movs ", buf);
            emit_write_from_r0();
            return {};
        }

        WordInR0 operator=(WordInR0 val) {
            emit_write_from_r0();
            return {};
        }

        WordInR0 operator=(ZeroExtendedByteInR0 val) {
            emit_write_from_r0();
            return {};
        }

        RestingStateVariable& operator=(const TState& t) {
            emit("mov ", "r0, r_t");
            emit_write_from_r0();
            return *this;
        }

        operator WordInR0() const {
            emit_read_to_r0();
            return {};
        }

        const char *name;
        int offset;
        int size;
    };

    template <enum Reg16Name R> struct Reg16 {
        Reg16<R>() : reg(R), thisref(R) { }

        // todo do we need this? or should we use Constorrseg16
        Reg16<R>(const int& v) : reg(R), thisref(R) {
            assert(R == TEMPORARY || R == TEMPORARY_RESTRICTED);
            set(v);
        }
        Reg16<R>(const WordInR0& v) : reg(R), thisref(R) {
            emit_reg_from_r0(R);
        }
        Reg16<R>(const ZeroExtendedByteInR0& v) : reg(R), thisref(R) {
            assert(R == TEMPORARY || R == TEMPORARY_RESTRICTED);
            char buf[32];
            snprintf(buf, sizeof(buf), "%s, r0", arm_regs[reg]);
            emit("mov ", buf);
        }
        Reg16<R>(const WordInR0HighWordUnknown& v) : reg(R), thisref(R) {
            char buf[32];
            emit("uxth", "r0, r0");
            snprintf(buf, sizeof(buf), "%s, r0", arm_regs[reg]);
            emit("mov ", buf);
        }

        Reg16<R> operator|=(const WordInR0& v) {
            assert(is_arm_lo_reg(R)); // no particular reason, just not used
            char buf[32];
            snprintf(buf, sizeof(buf), "%s, r0", arm_regs[reg]);
            emit("orrs ", buf);
            return *this;
        }

        Reg16<R> operator|=(const WordInR1& v) {
            assert(is_arm_lo_reg(R)); // no particular reason, just not used
            char buf[32];
            snprintf(buf, sizeof(buf), "%s, r1", arm_regs[reg]);
            emit("orrs ", buf);
            return *this;
        }

        Reg16<R> operator ++() {
            add(1);
            return *this;
        }
        // return void since we don't return the old value (still allows a standalone foo++)
        void operator ++(int) {
            add(1);
        }
        Reg16<R> operator --() {
            sub(1);
            return *this;
        }
        void operator --(int) {
            sub(1);
        }
        void add(int x) {
            assert(x > 0 && x < 256);
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "%s, #%d", arm_regs[reg], x);
                emit("adds ", buf);
                snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_regs[reg]);
                emit("uxth", buf);
            } else {
                emit_r0_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, #%d", x);
                emit("adds ", buf);
                emit("uxth", "r0, r0");
                emit_reg_from_r0(thisref);
            }
        }

        void sub(int x) {
            assert(x>=0 && x < 256);
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "%s, #%d", arm_regs[reg], x);
                emit("subs ", buf);
                snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_regs[reg]);
                emit("uxth", buf);
            } else {
                emit_r0_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, #%d", x);
                emit("subs ", buf);
                emit("uxth", "r0, r0");
                emit_reg_from_r0(thisref);
            }
        }
        operator Reg16Ref() const {
            return thisref;
        }
        void operator+=(int x) {
            add(x);
        }
        void operator-=(int x) {
            sub(x);
        }
        Reg16<R> operator+=(WordInR0HighWordUnknown x) {
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "%s, r0", arm_regs[reg]);
                emit("add ", buf);
                snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_regs[reg]);
                emit("uxth", buf);
            } else {
                emit_r1_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, r1");
                emit("add ", buf);
                emit("uxth", "r0, r0");
                emit_reg_from_r0(thisref);
            }
            return *this;
        }

        WordInR0 operator+(SignExtendedByteInR0HighWordUnknown x) const {
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "r0, %s", arm_regs[reg]);
                emit("add ", buf);
                emit("uxth", "r0, r0");
            } else {
                emit_r1_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, r1");
                emit("add ", buf);
                emit("uxth", "r0, r0");
            }
            return WordInR0();
        }

        WordInR0HighWordUnknown operator+(int x) const {
            assert(x == 1);
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "r0, %s, #%d", arm_regs[reg], x);
                emit("adds ", buf);
            } else {
                emit_r0_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, #%d", x);
                emit("adds ", buf);
            }
            return WordInR0HighWordUnknown();
        }

        WordInR0HighWordUnknown operator-(int x) const {
            assert(x == 1);
            char buf[128];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "r0, %s, #%d", arm_regs[reg], x);
                emit("subs ", buf);
            } else {
                emit_r0_from_reg(thisref);
                snprintf(buf, sizeof(buf), "r0, #%d", x);
                emit("subs ", buf);
            }
            return WordInR0HighWordUnknown();
        }

        Reg16& operator=(const Reg16Ref& s) {
            if (s.reg != reg)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_regs[s.reg]);
                emit("mov ", buf);
            }
            return *this;
        }

        Reg16& operator=(const RestingStateVariable& s) {
            *this = (WordInR0)s;
            return *this;
        }

        Reg16& operator=(int val) {
            set(val);
            return *this;
        }

        WordInR1 operator|(const WordInR1& v) const {
            char buf[32];
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "r1, %s", arm_regs[reg]);
                emit("orrs ", buf);
            } else {
                assert(false); // for now
            }
            return WordInR1();
        }

        void set(int x) {
            char buf[128];
            if (is_arm_lo_reg(reg)) {
                snprintf(buf, sizeof(buf), "%s, #%d", arm_regs[reg], x);
                emit("movs ", buf);
            } else {
                snprintf(buf, sizeof(buf), "r0, #%d", x);
                emit("movs ", buf);
                snprintf(buf, sizeof(buf), "%s, r0", arm_regs[reg]);
                emit("mov ", buf);
            }
        }
        operator WordInR0() const {
            return WordInR0(thisref);
        }
        operator WordInR1() const {
            return WordInR1(thisref);
        }
        enum Reg16Name reg;
        Reg16Ref thisref;
        word value;
    };

#define rs_var(name) static RestingStateVariable name;
#define rs_var_init(name) eZ80t::RestingStateVariable eZ80t::name(#name, sizeof(z80a_resting_state.name), rs_offsetof_##name)
#define rs_var_struct_init(s, name) eZ80t::RestingStateVariable eZ80t::s::name(#s "." #name, sizeof(z80a_resting_state.name), rs_offsetof_##s##_##name)

    static void assign_hi_byte_from_lo_byte_clear_r0(Reg8Ref ref) {
        assert(!ref.lo);
        Reg16Name reg = ref.reg;
        assert(reg != TEMPORARY && reg != TEMPORARY_RESTRICTED); // can't imagine anyone assigning to high byte of temporary
        emit_zero_extend_z80_lo_to_arm(reg, 1);
        emit("orrs ", "r0, r1");
        emit_reg_from_r0(ref);
    }

    static void assign_lo_byte_from_hi_byte_clear_r(Reg8Ref ref, const char*arm_reg_name) {
        assert(ref.lo);
        char buf[128];
        Reg16Name reg = ref.reg;
        if (reg == TEMPORARY) {
            snprintf(buf, sizeof(buf), "r_temp, %s", arm_reg_name);
            emit("mov ", buf, "// fine to overwrite hi in r_temp");
        } else if (reg == TEMPORARY_RESTRICTED) {
            snprintf(buf, sizeof(buf), "r_temp_restricted, %s", arm_reg_name);
            emit("mov ", buf, "// fine to overwrite hi in r_temp_restricted");
        } else if (is_arm_lo_reg(reg))
        {
            snprintf(buf, sizeof(buf), "%s, #8", arm_regs[reg]);
            emit("lsls ", buf);
            emit("lsrs ", buf);
            snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_reg_name);
            emit("orrs ", buf);
        } else {
            emit_r1_from_reg(reg);
            emit("lsrs ", "r1, #8");
            emit("lsls ", "r1, #8");
            snprintf(buf, sizeof(buf), "r1, %s", arm_reg_name);
            emit("orrs ", buf);
            emit_reg_from_r1(ref);
        }
    }

    friend RegLo<AF>& operator|=(RegLo<AF>& r, int val) {
        char buf[32];
        assert(val>=0 && val<= 255);
        snprintf(buf, sizeof(buf), "r0, #%d", val);
        emit("movs ", buf);
        emit("orrs ", "r_af, r0");
        return r;
    }

    friend RegLo<AF>& operator&=(RegLo<AF>& r, int val) {
        char buf[32];
        if (val < 0) {
            val = ~val;
            assert(val >= 0 && val <= 255);
            snprintf(buf, sizeof(buf), "r0, #%d", val);
            emit("movs ", buf);
            emit("bics ", "r_af, r0");
            return r;
        } else
        {
            assert(val >= 0 && val <= 255);
            snprintf(buf, sizeof(buf), "r0, #%d", val);
            emit("movs ", buf);
            emit("ands ", "r_af, r0");
            return r;
        }
    }

    friend LoByteAndGarbageInR0 operator+(RegLo<TEMPORARY>& r, LoByteAndGarbageInR0 v) {
        emit("add ", "r0, r_temp");
        return v;
    }

    friend LoByteAndGarbageInR0 operator+(LoByteAndGarbageInR0 l, int val) {
        char buf[32];
        assert(val>=0 && val<= 255);
        snprintf(buf, sizeof(buf), "r0, #%d", val);
        emit("adds ", buf);
        return l;
    }

    friend LoByteAndGarbageInR0 operator-(LoByteAndGarbageInR0 l, int val) {
        char buf[32];
        assert(val>=0 && val<= 255);
        snprintf(buf, sizeof(buf), "r0, #%d", val);
        emit("subs ", buf);
        return l;
    }

    template <enum Reg16Name R> struct RegHi {
        RegHi<R>() : reg(R), thisref(R, false) { }
        RegHi<R>(ZeroExtendedByteInR0 v) : reg(R), thisref(R, false) {
            emit("lsls", "r0, #8");
            assign_hi_byte_from_lo_byte_clear_r0();
        }

        operator Reg8Ref() {
            return thisref;
        }

        void assign_hi_byte_from_lo_byte_clear_r0() {
            eZ80t::assign_hi_byte_from_lo_byte_clear_r0(thisref);
        }

        template <enum Reg16Name S> RegHi<R>& operator=(const RegHi<S>& s) {
            emit_r0_from_reg_hi(s.thisref);
            emit("lsls ", "r0, #8");
            assign_hi_byte_from_lo_byte_clear_r0();
            return *this;
        }

        template <enum Reg16Name S> RegHi<R>& operator=(const RegLo<S>& s) {
            emit_zero_extend_z80_lo_to_arm(s.reg, 0);
            emit("lsls", "r0, #8");
            assign_hi_byte_from_lo_byte_clear_r0();
            return *this;
        }

        ZeroExtendedByteInR0 operator&(int v) {
            ZeroExtendedByteInR0 rc = *this;
            assert(v>=0 && v<=255);
            char buf[32];
            snprintf(buf, sizeof(buf), "r1, #0x%02x", v);
            emit("movs ", buf);
            emit("ands ", "r0, r1");
            return rc;
        }

        operator ZeroExtendedByteInR0() const {
            return ZeroExtendedByteInR0(thisref);
        }

        operator LoByteAndGarbageInR0() const {
            return LoByteAndGarbageInR0(thisref);
        }

        WordInR1 operator<<(int v) {
            assert( v == 8);
            if (is_arm_lo_reg(reg))
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "r1, %s, #8", arm_regs[reg]);
                emit("lsrs ", buf);
            } else {
                emit_r1_from_reg_hi(thisref);
                emit("lsrs ", "r1, #8");
            }
            emit("lsls ", "r1, #8");
            return WordInR1();
        }

        RegHi<R> operator --() {
            sub(1);
            return *this;
        }

        RegHi<R> operator^=(int v) {
            char buf[32];
            assert(v >=0 && v <= 255);
            assert(R == AF);
            snprintf(buf, sizeof(buf), "r0, #%d", v);
            emit("movs ", buf);
            emit("lsls ", "r0, #8");
            emit("eors ", "r_af, r0");
            return *this;
        }

        void sub(int x) {
            assert(x>=0 && x < 256);
            char buf[128];
            sprintf(buf, "r1, #%d", x);
            emit("movs ", buf);
            emit("lsls ", "r1, #8");
            if (is_arm_lo_reg(reg))
            {
                snprintf(buf, sizeof(buf), "%s, r1", arm_regs[reg]);
                emit("subs ", buf);
                snprintf(buf, sizeof(buf), "%s, %s", arm_regs[reg], arm_regs[reg]);
                emit("uxth", buf);
            } else {
                emit_r0_from_reg(thisref);
                emit("subs ", "r0, r1");
                emit("uxth", "r0, r0");
                emit_reg_from_r0(thisref);
            }
        }

        enum Reg16Name reg;
        Reg8Ref thisref;
    };

    template <enum Reg16Name R> struct RegLo
    {
        RegLo<R>() : reg(R), thisref(R, true)
        {
        }

        RegLo<R>(ZeroExtendedByteInR0 v) : reg(R), thisref(R, true)
        {
            assign_lo_byte_from_hi_byte_clear_r0();
        }

        void assign_lo_byte_from_hi_byte_clear_r0() {
            eZ80t::assign_lo_byte_from_hi_byte_clear_r(thisref, "r0");
        }

        operator Reg8Ref() {
            return thisref;
        }

        operator ZeroExtendedByteInR0() const {
            return ZeroExtendedByteInR0(thisref);
        }

        operator LoByteAndGarbageInR0() const {
            return LoByteAndGarbageInR0(thisref);
        }

        template <enum Reg16Name S> RegLo<R>& operator=(const RegLo<S>& s) {
            emit_zero_extend_z80_lo_to_arm(s.reg, 0);
            assign_lo_byte_from_hi_byte_clear_r0();
            return *this;
        }

        template <enum Reg16Name S> RegLo<R>& operator=(RegHi<S> s) {
            emit_r0_from_reg_hi(s);
            assign_lo_byte_from_hi_byte_clear_r0();
            return *this;
        }

        RegLo<R>& operator=(const WordInR0HighWordUnknown& s)
        {
            if (reg == TEMPORARY) {
                WordInR0 __unused x = s;
                emit("mov ", "r_temp, r0","// fine to overwrite hi in r_temp");
            } else if (reg == TEMPORARY_RESTRICTED) {
                __unused WordInR0 x = s;
                emit("mov ", "r_temp_restricted, r0","// fine to overwrite hi in r_temp");
            } else
            {
                emit("uxtb", "r0, r0");
                assign_lo_byte_from_hi_byte_clear_r0();
            }
            return *this;
        }

        enum Reg16Name reg;

        Reg8Ref thisref;
    };

    typedef Reg16<TEMPORARY> temp16;
    typedef RegLo<TEMPORARY> temp8;
    typedef WordInR1 temp16_2;
    // the scratch must be global
#define scratch16
    typedef Reg16<TEMPORARY_RESTRICTED> temp16_xy;
    typedef RegLo<TEMPORARY_RESTRICTED> temp8_xy;
    typedef SignExtendedByteInR0HighWordUnknown address_delta;

    TState t;
    static const char *arm_regs[REG_COUNT];

	eZ80t();

    typedef void (eZ80t::*CALLFUNC)();
//    typedef temp8 (eZ80t::*CALLFUNCI)(temp8);

    void generate_arm();

protected:
	ZeroExtendedByteInR0 IoRead(WordInR0 port) const;
	void IoWrite(WordInR1 port, LoByteAndGarbageInR0 v);
    ZeroExtendedByteInR0 Read(WordInR0 addr) const;
    ZeroExtendedByteInR0 ReadInc(Reg16Ref addr) const;
    WordInR0 Read2(WordInR0 addr) const; // low then high
    WordInR0 Read2Inc(Reg16Ref addr) const; // low then high
	void Write(Reg16Ref addr, LoByteAndGarbageInR0 v);
    void WriteXY(WordInR0 addr, Reg8Ref v);
    void Write2(Reg16Ref addr, WordInR0 v); // low then high
    void dump_instructions(const char *prefix, const CALLFUNC *instrs, int count, const char *description=NULL, int mult = 1);

    static const char *pending_call;
    static void emit_pending_call() {
        if (pending_call) {
            const char *funcname = pending_call;
            pending_call = nullptr;
            emit_save_lr_if_not_done_already();
            emit("bl  ", funcname);
        }
    }
    static void emit(const char *a) {
        emit_pending_call();
        printf("%s\n", a);
    }
    static void emit(const char *a, const char *b, const char *c = "") {
        char buf[128];
        snprintf(buf, sizeof(buf), "\t%s %s\t\t%s", a, b, c);
        emit(buf);
    }
    static void emit_zero_extend_z80_lo_to_arm(RegRef lo, int arm_reg) {
        char buf[32];
        if (is_arm_lo_reg(lo.reg))
        {
            snprintf(buf, sizeof(buf), "r%d, %s", arm_reg, arm_regs[lo.reg]);
            emit("uxtb", buf);
        } else {
            if (arm_reg == 0) {
                emit_r0_from_reg(lo);
            } else if (arm_reg == 1) {
                emit_r1_from_reg(lo);
            } else if (arm_reg == R_TEMP_ARM) {
                emit_r_temp_from_reg(lo);
            } else {
                assert(false);
            }
            snprintf(buf, sizeof(buf), "r%d, r%d", arm_reg, arm_reg);
            emit("uxtb", buf);
        }
    }

    static void emit_r0_from_reg(const RegRef &x) {
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, %s", arm_regs[x.reg]);
        emit("mov ", buf);
    }

    static void emit_r_temp_from_reg(const RegRef &x) {
        char buf[32];
        snprintf(buf, sizeof(buf), "r_temp, %s", arm_regs[x.reg]);
        emit("mov ", buf);
    }

    static void emit_r_temp_from_reg8(const Reg8Ref &x) {
        char buf[32];
        if (x.lo) {
            emit_zero_extend_z80_lo_to_arm(x, R_TEMP_ARM);
        } else {
            if (is_arm_lo_reg(x.reg))
            {
                snprintf(buf, sizeof(buf), "r_temp, %s, #8", arm_regs[x.reg]);
                emit("lsrs ", buf);
            } else {
                emit_r_temp_from_reg((RegRef)x);
                emit("lsrs ", "r_temp, #8");
            }
        }
    }

    static void emit_reg8_from_r_temp(const Reg8Ref &x) {
        if (x.lo) {
            assign_lo_byte_from_hi_byte_clear_r(x, "r_temp");
        } else {
            emit("lsls ", "r0, r_temp, #8");
            assign_hi_byte_from_lo_byte_clear_r0(x);
        }
    }

    static void emit_r0_from_reg_lo(const Reg8Ref &x) {
        assert(x.lo);
        emit_zero_extend_z80_lo_to_arm(x, 0);
    }

    static void emit_r1_from_reg_lo(const Reg8Ref &x) {
        assert(x.lo);
        emit_zero_extend_z80_lo_to_arm(x, 1);
    }

    static void emit_r0_from_reg8(const Reg8Ref &x) {
        if (x.lo) {
            emit_r0_from_reg_lo(x);
        } else {
            emit_r0_from_reg_hi(x);
        }
    }
    static void emit_reg8_from_r0(const Reg8Ref &x) {
        if (x.lo) {
            assign_lo_byte_from_hi_byte_clear_r(x, "r0");
        } else {
            emit("lsls ", "r0, #8");
            assign_hi_byte_from_lo_byte_clear_r0(x);
        }
    }

    static void emit_r0_from_reg_hi(const Reg8Ref &x) {
        char buf[32];
        assert(!x.lo);
        if (is_arm_lo_reg(x.reg))
        {
            snprintf(buf, sizeof(buf), "r0, %s, #8", arm_regs[x.reg]);
            emit("lsrs ", buf);
        } else {
            emit_r0_from_reg(x);
            emit("lsrs ", "r0, #8");
        }
    }
    static void emit_r1_from_reg_hi(const Reg8Ref &x) {
        char buf[32];
        assert(!x.lo);
        if (is_arm_lo_reg(x.reg))
        {
            snprintf(buf, sizeof(buf), "r1, %s, #8", arm_regs[x.reg]);
            emit("lsrs ", buf);
        } else {
            emit_r1_from_reg(x);
            emit("lsrs ", "r1, #8");
        }
    }
    static void emit_reg_from_r0(const RegRef &x) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s, r0", arm_regs[x.reg]);
        emit("mov ", buf);
    }
    static void emit_reg_from_r1(const RegRef &x) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s, r1", arm_regs[x.reg]);
        emit("mov ", buf);
    }
    static void emit_r1_from_reg(const RegRef &x) {
        char buf[32];
        snprintf(buf, sizeof(buf), "r1, %s", arm_regs[x.reg]);
        emit("mov ", buf);
    }

    template <typename A, typename B> void if_nonzero(ZeroExtendedByteInR0 v, A&& a, B&& b) {
        // can pass zero extended value
        if_nonzero(WordInR0(), a, b);
    }

    template <typename A, typename B> void if_nonzero(WordInR0 v, A&& a, B&& b) {
        emit("cmp ", "r0, #0");
        emit("beq ", "1f");
        int lr = lr_saved;
        a();
        emit_function_return();
        lr_saved = lr;
        emit("1:");
        b();
    }

    template <typename A> void if_nonzero(WordInR0 v, A&& a) {
        emit("cmp ", "r0, #0");
        emit("beq ", "1f");
        a();
        emit("1:");
    }

    template <typename A> void if_zero(ZeroExtendedByteInR0 v, A&& a) {
        if_zero((WordInR0)v, a);
    }

    template <typename A> void if_zero(WordInR0 v, A&& a) {
        if_equal(v, 0, a);
    }

    template <typename A> void if_equal(ZeroExtendedByteInR0 v, int cmp, A&& a) {
        if_equal((WordInR0)v, cmp, a);
    }

    template <typename A> void if_equal(WordInR0 v, int cmp, A&& a) {
        char buf[128];
        assert(cmp >=0 && cmp <=255);
        snprintf(buf, sizeof(buf), "r0, #%d", cmp);
        emit("cmp ", buf);
        emit("bne ", "1f");
        a();
        emit("1:");
    }

    template <typename A, typename B> void if_flag_set(byte flag, A&& a, B&& b) {
        assert(__builtin_popcount(flag) == 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, r_af, #%d", 1 + __builtin_ctz(flag));
        emit("lsrs ", buf);
        emit("bcc ", "2f");
        int lr = lr_saved;
        a();
        emit_function_return();
        lr_saved = lr;
        emit("2:");
        b();
    }

    template <typename A> void if_flag_set(byte flag, A&& a) {
        assert(__builtin_popcount(flag) == 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, r_af, #%d", 1 + __builtin_ctz(flag));
        emit("lsrs ", buf);
        emit("bcc ", "2f");
        a();
        emit("2:");
    }


    template <typename A, typename B> void if_flag_clear(byte flag, A&& a, B&& b) {
        assert(__builtin_popcount(flag) == 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, r_af, #%d", 1 + __builtin_ctz(flag));
        emit("lsrs ", buf);
        emit("bcs ", "3f");
        int lr = lr_saved;
        a();
        emit_function_return();
        lr_saved = lr;
        emit("3:");
        b();
    }

    template <typename A> void if_flag_clear(byte flag, A&& a) {
        assert(__builtin_popcount(flag) == 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, r_af, #%d", 1 + __builtin_ctz(flag));
        emit("lsrs ", buf);
        emit("bcs ", "3f");
        a();
        emit("3:");
    }

#define set_a35_flags_preserve_set(preserve, set) _set_a35_flags_preserve_set(#preserve, #set)

    void _set_a35_flags_preserve_set(const char *preserve, const char *set) {
        char buf[128];
        snprintf(buf, sizeof(buf), "r0, %s", preserve);
        emit("preserve_only_flags", buf);
        snprintf(buf, sizeof(buf), "r_af, #%s", set);
        emit("adds ", buf);

        ZeroExtendedByteInR0 __unused r0 = a;
        emit("movs ", "r1, #(F3|F5)");
        emit("ands ", "r0, r1");
        emit("orrs ", "r_af, r0");
    }

#define set_logic_flags_preserve(reg, preserve) _set_logic_flags_preserve_reset(reg, #preserve, NULL)
#define set_logic_flags_preserve_reset(reg, preserve, reset) _set_logic_flags_preserve_reset(reg, #preserve, #reset)

    void _set_logic_flags_preserve_reset(ZeroExtendedByteInR0 value, const char *preserve_flags, const char* reset_flags) {
        char buf[128];
        snprintf(buf, sizeof(buf), "r1, %s", preserve_flags);
        emit("preserve_only_flags", buf);
        emit("ldr ", "r1, =_log_f");
        emit("ldrb", "r0, [r1, r0]");
        emit("orrs ", "r_af, r0");
        if (reset_flags) {
            snprintf(buf, sizeof(buf), "r1, #%s", reset_flags);
            emit("movs ", buf);
            emit("bics ", "r_af, r1");
        }
    }

    static bool r_temp_used;
    static bool r_temp_restricted_used;

    void reset_function_state() {
        r_temp_used = r_temp_restricted_used = false;
        lr_saved = false;
    }
    void emit_function_return() {
        if (pending_call && !lr_saved) {
            // tail call (note we don't tail call when lr saved, since we can't pop lr

            // don't ever use r2 for arg (I hope)
            char buf[32];
            // almost certainly too far for plain "b label"
            snprintf(buf, sizeof(buf), "r2, =%s", pending_call);
            pending_call = NULL; // reset so we don't recurse
            emit("ldr ", buf);
            emit("bx  ", "r2");
            emit("");
            return;
        }
        emit_pending_call();
        if (lr_saved) {
            emit("pop ", "{pc}");
        } else
        {
            emit("bx  ", "lr");
        }
        emit("");
    }
    static void emit_save_lr_if_not_done_already() {
        if (!lr_saved)
        {
            emit("push", "{lr}");
            lr_saved = true;
        }
    }
    static bool lr_saved;
    static void emit_call_func(const char *funcname) {
        pending_call = funcname;
    }
    static void emit_call_func_with_reg(RegRef &x, const char *funcname) {
        emit_r0_from_reg(x);
        emit_call_func(funcname);
        emit_reg_from_r0(x);
    }

    void inc8(Reg8Ref x)
    {
        emit_r0_from_reg8(x);
        emit_call_func("inc8");
        emit_reg8_from_r0(x);
    }
    void dec8(Reg8Ref x)
    {
        emit_r0_from_reg8(x);
        emit_call_func("dec8");
        emit_reg8_from_r0(x);
    }
    void rlc8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("rlc8");
        emit_reg8_from_r0(x);
    }
    void rrc8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("rrc8");
        emit_reg8_from_r0(x);
    }
    void rl8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("rl8");
        emit_reg8_from_r0(x);
    }
    void rr8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("rr8");
        emit_reg8_from_r0(x);
    }
    void sla8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("sla8");
        emit_reg8_from_r0(x);
    }
    void sli8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("sli8");
        emit_reg8_from_r0(x);
    }
    void sra8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("sra8");
        emit_reg8_from_r0(x);
    }
    void srl8(Reg8Ref x) {
        emit_r0_from_reg8(x);
        emit_call_func("srl8");
        emit_reg8_from_r0(x);
    }
#ifdef USE_LARGER_FASTER_CB
    void bit(Reg8Ref x, byte bit)
    {
        // this function is large, so call the corrsesponding ls function
        emit_r_temp_from_reg8(x);
        char buf[32];
        assert(bit >= 0 && bit <= 7);
        snprintf(buf, sizeof(buf), "opli_r35_%02x", 8*(8 + bit)); // note 8-15 are bit functions
        emit_call_func(buf);
        emit_reg8_from_r_temp(x);
    }
    void bitmem(ZeroExtendedByteInR0 src, byte bit)
    {
        // this function is large, so call the corrsespdnding ls function
        emit("mov ", "r_temp, r0");
        char buf[32];
        assert(bit >= 0 && bit <= 7);
        snprintf(buf, sizeof(buf), "opli_m35_%02x", 8*(8 + bit)); // note 8-15 are bit functions
        emit_call_func(buf);
    }
    // These are also slightly faster than the 8x32 versions since they can work on values in place (e.g. high reg)
    void res(Reg8Ref x, byte bit) const
    {
        if (!x.lo)
        {
            bit += 8;
        }
        char buf[32];
        if (bit < 8)
        {
            snprintf(buf, sizeof(buf), "r0, #0x%02x", 1 << bit);
            emit("movs ", buf);
        } else {
            emit("movs ", "r0, #1");
            snprintf(buf, sizeof(buf), "r0, #%d", bit);
            emit("lsls ", buf);
        }
        if (is_arm_lo_reg(x.reg)) {
            snprintf(buf, sizeof(buf), "%s, r0", arm_regs[x.reg]);
            emit("bics ", buf);
        } else
        {
            emit_r1_from_reg(x);
            emit("bics ", "r1, r0");
            emit_reg_from_r1(x);
        }
    }
    void set(Reg8Ref x, byte bit) const
    {
        if (!x.lo)
        {
            bit += 8;
        }
        char buf[32];
        if (bit < 8)
        {
            snprintf(buf, sizeof(buf), "r0, #0x%02x", 1 << bit);
            emit("movs ", buf);
        } else {
            emit("movs ", "r0, #1");
            snprintf(buf, sizeof(buf), "r0, #%d", bit);
            emit("lsls ", buf);
        }
        if (is_arm_lo_reg(x.reg)) {
            snprintf(buf, sizeof(buf), "%s, r0", arm_regs[x.reg]);
            emit("orrs ", buf);
        } else
        {
            emit_r1_from_reg(x);
            emit("orrs ", "r1, r0");
            emit_reg_from_r1(x);
        }
    }
#endif
    void add8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("add8");
    }
    void adc8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("adc8");
    }
    void sub8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("sub8");
    }
    void sbc8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("sbc8");
    }
    void and8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("ands8");
    }
    void or8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("or8");
    }
    void xor8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("xor8");
    }
    void cp8(ZeroExtendedByteInR0 src)
    {
        emit_call_func("cp8");
    }

    void OpeA2A3AAABFlags(const RegLo<TEMPORARY>& val, LoByteAndGarbageInR0 tmp)
    {
        emit("// a2a3aaabflags r_temp, r0", "");

        // f = 0;
        emit("lsrs ", "r_af, #8");
        emit("lsls ", "r_af, #8");

//        f = log_f[b] & ~PV;
        emit("mov ", "r2, r_bc");
        emit("lsrs ", "r2, #8");
        emit("ldr ", "r1, =_log_f");
        emit("ldrb", "r2, [r1, r2]");
        emit("movs ", "r1, #PV");
        emit("bics ", "r2, r1");
        emit("orrs ", "r_af, r2");

        //        if(tmp < val) f |= (HF|CF);
        emit("uxtb", "r0, r0");
        emit("cmp ", "r0, r_temp");
        emit("bge ", "1f");
        emit("adds ", "r_af, #HF|CF");
        emit("1:");

        //        if(val & 0x80) f |= NF;
        emit("lsrs ", "r1, r_temp, #8");
        emit("bcc ", "1f");
        emit("adds ", "r_af, #NF");
        emit("1:");

//        if(log_f[(tmp & 0x07) ^ b] & PV) f |= PV;
        emit("lsls ", "r1, r0, #29");
        emit("lsrs ", "r1, #21");
        emit("mov ", "r2, r_bc");
        emit("eors ", "r1, r2");
        emit("lsrs ", "r1, #8");
        emit("ldr ", "r0, =_log_f");
        emit("ldrb", "r0, [r0, r1]");
        emit("movs ", "r1, #PV");
        emit("ands ", "r0, r1");
        emit("orrs ", "r_af, r0");
    }

    // --- logic (cb) ops --------------------------------------

    // Note these are the versions that work on r_temp

    void Opl_rlc() {
        rlc8(temp_lo);
    }

    void Opl_rrc() {
        rrc8(temp_lo);
    }

    void Opl_rl() {
        rl8(temp_lo);
    }

    void Opl_rr() {
        rr8(temp_lo);
    }

    void Opl_sla() {
        sla8(temp_lo);
    }

    void Opl_sra() {
        sra8(temp_lo);
    }

    void Opl_sli() {
        sli8(temp_lo);
    }

    void Opl_srl() {
        srl8(temp_lo);
    }

    void Opl_bit0() {
        bit_regular_35(0);
    }

    void Opl_bit1() {
        bit_regular_35(1);
    }

    void Opl_bit2() {
        bit_regular_35(2);
    }

    void Opl_bit3() {
        bit_regular_35(3);
    }

    void Opl_bit4() {
        bit_regular_35(4);
    }

    void Opl_bit5() {
        bit_regular_35(5);
    }

    void Opl_bit6() {
        bit_regular_35(6);
    }

    void Opl_bit7() {
        bit_regular_35(7);
    }

    void Opl_bit0m() {
        bit_mem_h_35(0);
    }

    void Opl_bit1m() {
        bit_mem_h_35(1);
    }

    void Opl_bit2m() {
        bit_mem_h_35(2);
    }

    void Opl_bit3m() {
        bit_mem_h_35(3);
    }

    void Opl_bit4m() {
        bit_mem_h_35(4);
    }

    void Opl_bit5m() {
        bit_mem_h_35(5);
    }

    void Opl_bit6m() {
        bit_mem_h_35(6);
    }

    void Opl_bit7m() {
        bit_mem_h_35(7);
    }

    void Opl_res0() {
        res_rtemp(0);
    }

    void Opl_res1() {
        res_rtemp(1);
    }

    void Opl_res2() {
        res_rtemp(2);
    }

    void Opl_res3() {
        res_rtemp(3);
    }

    void Opl_res4() {
        res_rtemp(4);
    }

    void Opl_res5() {
        res_rtemp(5);
    }

    void Opl_res6() {
        res_rtemp(6);
    }

    void Opl_res7() {
        res_rtemp(7);
    }

    void Opl_set0() {
        set_rtemp(0);
    }

    void Opl_set1() {
        set_rtemp(1);
    }

    void Opl_set2() {
        set_rtemp(2);
    }

    void Opl_set3() {
        set_rtemp(3);
    }

    void Opl_set4() {
        set_rtemp(4);
    }

    void Opl_set5() {
        set_rtemp(5);
    }

    void Opl_set6() {
        set_rtemp(6);
    }

    void Opl_set7() {
        set_rtemp(7);
    }

    void res_rtemp(byte bit) const
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, #0x%02x", 1<<bit);
        emit("movs ", buf);
        emit("bics ", "r_temp, r0");
    }

    void set_rtemp(byte bit) const
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "r0, #0x%02x", 1<<bit);
        emit("movs ", buf);
        emit("orrs ", "r_temp, r0");
    }

    void bit_regular_35(byte bit)
    {
        // f = log_f[src & (1 << bit)] | HF | (f & CF) | (src & (F3|F5));
        emit("", "// this is the regular version of the inner bit code - it uses r_temp for F3 & F5");
        char buf[32];
        emit("preserve_only_flags", "r0, CF");
        snprintf(buf, sizeof(buf), "r0, #0x%02x", 1<<bit);
        emit("movs ", buf);
        emit("ands ", "r0, r_temp");

        emit("ldr ", "r1, =_log_f");
        emit("ldrb", "r0, [r1, r0]");
        emit("orrs ", "r_af, r0");
        emit("adds ", "r_af, #HF");

        emit("movs ", "r0, #(F3|F5)");
        emit("ands ", "r0, r_temp");
        emit("orrs ", "r_af, r0", "// note this OR is safe because 35 from log_f[r_temp&mask] are <=");
    }
        
    void bit_mem_h_35(byte bit)
    {
        // f = log_f[src & (1 << bit)] | HF | (f & CF);
        // f = (f & ~(F3|F5)) | (mem_h & (F3|F5));

        emit("", "// Beware confusion; t may already be updated for the instruction");
        emit("", "// this is the bitmem version of the inner bit code - it uses mem_h for F3 & F5");
        char buf[32];
        emit("preserve_only_flags", "r0, CF");
        snprintf(buf, sizeof(buf), "r0, #0x%02x", 1<<bit);
        emit("movs ", buf);
        emit("ands ", "r0, r_temp");

        emit("ldr ", "r1, =_log_f");
        emit("ldrb", "r0, [r1, r0]");
        emit("orrs ", "r_af, r0");
        emit("adds ", "r_af, #HF");

        // use memptr for F3 & F5
        emit("movs ", "r1, #(F3|F5)");
        emit("bics ", "r_af, r1");
        emit_r0_from_reg_hi(mem_h);
        emit("ands ", "r0, r1");
        emit("orrs ", "r_af, r0");
    }

    template <enum Reg16Name R> void add16(Reg16<R> regM, Reg16Ref regN) {
        memptr = regM+1;
        emit_r0_from_reg(regM);
        emit_r1_from_reg(regN);
        emit_call_func("add16");
        emit_reg_from_r0(regM);
        t += 7;
    }

    void adc16(Reg16Ref reg) {
        memptr = hl+1;
        emit_r0_from_reg(reg);
        emit_call_func("adc16");
        t += 7;
    }

    // hl, reg
    void sbc16(Reg16Ref reg) {
        memptr = hl+1;
        emit_r0_from_reg(reg);
        emit_call_func("sbc16");
        t += 7;
    }

    void push(Reg16Ref v) const {
        emit_r0_from_reg(v);
        emit_call_func("_push");
    }

#include "../z80/z80_op_noprefix.h"

#ifdef USE_LARGER_FASTER_CB
	#include "../z80/z80_op_cb.h"
#endif
	#include "../z80/z80_op_dd.h"
    #include "../z80/z80_op_fd.h"
	#include "../z80/z80_op_ed.h"

	// hand coded instructions
	void Op3F() { // ccf
        emit("movs ", "r1, #CF");
        emit("lsrs ", "r0, r_af, #1");
        emit("bcc ", "1f");
        emit("movs ", "r1, #HF");
        emit("1:");
        emit("preserve_only_flags", "r0, PV|ZF|SF");
        emit("orrs ", "r_af, r1");
        emit("lsrs ", "r0, r_af, #8");
        emit("movs ", "r1, #(F3|F5)");
        emit("ands ", "r0, r1");
        emit("orrs ", "r_af, r0");
    }

	void OpED()
    {
        emit_save_lr_if_not_done_already();
        emit("ldr ","r_temp, =ope_table");
        emit("step_op_table_in_r_temp_maybe_neg","");
    }

    void OpDD()
    {
        emit("movs ", "r0, #0xdd");
        emit_call_func("opDDFD");
    }

    void OpFD()
    {
        emit("movs ", "r0, #0xfd");
        emit_call_func("opDDFD");
    }

    void OpCB()
    {
#ifndef USE_LARGER_FASTER_CB
        emit("", "// note this is a slow implementation which is 4K smaller... define USE_LARGER_FASTER_CB for speed");
        emit("fetch", "");
        // r1 is inner instruction
        emit("lsrs ", "r2, r0, #3");
        emit("lsls ", "r2, #1");

        emit("movs ", "r1, #7");
        emit("ands ", "r1, r0", "// r1 is outer instruction");

        emit("cmp ", "r1, #6", "// check for memory operand");
        emit("bne ", "2f");
        emit("ldr ", "r_temp, =opli_m35_table");
        emit("b   ", "3f");
        emit("2:");
        emit("ldr ", "r_temp, =opli_r35_table");
        emit("3:");
        emit("ldrh", "r2, [r_temp, r2]");
        emit("sxth", "r2, r2");
        emit("add ", "r2, r_temp");

        emit("ldr ", "r_temp, =opcbX_table");
        emit("lsrs ", "r0, #7");
        emit("bcc ","1f", "// arg no single test for this flag combo");
        emit("bne ","1f");
        emit("adds ", "r_temp, #(opcbbitX_table - opcbX_table)");
        emit("1:");
        emit("lsls ", "r0, r1, #1");
        emit("ldrh", "r1, [r_temp, r0]");
        emit("sxth", "r1, r1");
        emit("add ", "r1, r_temp");

        emit("bx  ", "r1");
#else
        emit("ldr ", "r_temp, =opl_table");
        emit("fetch", "");

        emit("lsls ", "r0, #1");
        emit("ldrh", "r0, [r_temp, r0]");
        emit("sxth", "r0, r0");
        emit("add ", "r0, r_temp");
        emit("bx  ", "r0");
#endif
    }

    void OpD9() { // exx
        // todo this isn't strictly necessary to hard code, but is slightly better than generated code
        emit("ldr ", "r2, =z80a_resting_state");
        emit("mov ", "r0, r_bc");
        alt.bc.emit_read(1, 2);
        alt.bc.emit_write(0, 2);
        emit("mov ", "r_bc, r1");
        emit("mov ", "r0, r_de");
        alt.de.emit_read(1, 2);
        alt.de.emit_write(0, 2);
        emit("mov ", "r_de, r1");
        emit("mov ", "r0, r_hl");
        alt.hl.emit_read(1, 2);
        alt.hl.emit_write(0, 2);
        emit("mov ", "r_hl, r1");
    }

    void Op07() { // rlca
        emit("movs ", "r0, #(SF|ZF|PV)");
        emit("ands ", "r0, r_af",                   "// r0 = f & (SF|ZF|PV)");
        emit("movs ", "r1, #0");
        emit("lsrs ", "r_af, #8");
        emit("lsls ", "r_af, #25");
        emit("bcc ", "1f");
        emit("movs ", "r1, #CF");
        emit("orrs ", "r0, r1",                     "// r0 = (f & (SF|ZF|PV)) | (a7 ? CF: 0)");
        emit("1:");
        emit("lsrs ", "r_af, #24");
        emit("orrs ", "r_af, r1");
        emit("movs ", "r1,  #(F3|F5)",              "// r_af = (a << 1) | (a7 ? 1 : 0)");
        emit("ands ", "r1, r_af");
        emit("lsls ", "r_af, #8",                   "// r_af = a' : 0");
        emit("orrs ", "r_af, r0",                   "// r_af = a' | f'");
        emit("orrs ", "r_af, r1",                   "// r_af |= 35 from a'");
    }

    void Op0F() { // rrca
        emit("movs ", "r0, #(SF|ZF|PV)");
        emit("ands ", "r0, r_af",                   "// r0 = f & (SF|ZF|PV)");
        emit("lsrs ", "r_af, r_af, #9");
        emit("bcc ", "1f");
        emit("adds ", "r0, #CF",                    "// r0 = (f & (SF|ZF|PV)) | (a0 ? CF: 0)");
        emit("adds ", "r_af, #128",                 "// r_af = (a0 ? 0x80 : 0) | (a >> 1)");
        emit("1:");
        emit("movs ", "r1, #(F3|F5)");
        emit("ands ", "r1, r_af");
        emit("lsls ", "r_af, #8",                   "// r_af = a' : 0");
        emit("orrs ", "r_af, r0",                   "// r_af = a' | f'");
        emit("orrs ", "r_af, r1",                   "// r_af |= 35 from a'");
    }

    void Op17() { // rla
        emit("movs ", "r0, #(SF|ZF|PV)");
        emit("ands ", "r0, r_af",                   "// r0 = f & (SF|ZF|PV)");
        emit("movs ", "r1, #CF");
        emit("ands ", "r1, r_af");
        emit("lsrs ", "r_af, #8");
        emit("lsls ", "r_af, #25");
        emit("bcc ", "1f");
        emit("adds ", "r0, #CF",                    "// r0 = (f & (SF|ZF|PV)) | (a7 ? CF: 0)");
        emit("1:");
        emit("lsrs ", "r_af, #24");
        emit("orrs ", "r_af, r1");
        emit("movs ", "r1,  #(F3|F5)",              "// r_af = (a << 1) | (c ? 1 : 0)");
        emit("ands ", "r1, r_af");
        emit("lsls ", "r_af, #8",                   "// r_af = a' : 0");
        emit("orrs ", "r_af, r0",                   "// r_af = a' | f'");
        emit("orrs ", "r_af, r1",                   "// r_af |= 35 from a'");
    }

    void Op1F() { // rra
        emit("movs ", "r0, #(SF|ZF|PV)");
        emit("ands ", "r0, r_af",                   "// r0 = f & (SF|ZF|PV)");
        emit("lsls ", "r2, r_af, #15",              "// r2 = (garbage : oldCF) << 15");
        emit("lsrs ", "r1, r_af, #9");
        emit("bcc ", "1f");
        emit("adds ", "r0, #CF",                    "// r0 = (f & (SF|ZF|PV)) | (a0 ? CF: 0)");
        emit("1:");
        emit("lsls ", "r_af, r1, #8");
        emit("orrs ", "r_af, r2");
        emit("orrs ", "r_af, r0");
        emit("movs ", "r0, #(F3|F5)");
        emit("uxth", "r_af, r_af");
        emit("ands ", "r0, r1"); // note we are anding with only 7 bits of A, but they include those we care about
        emit("orrs ", "r_af, r0");
    }

    void Op27() { // daa
//        int delta = 0;
//        int newflags = f&(CF|NF);

        emit("movs ", "r0, #0", "// delta");
        emit("movs ", "r_temp, #(CF|NF)");
        emit("ands ", "r_temp, r_af", "// new CHN flags - CH may be set later");

//        if ((f & HF) || (a & 0xf) > 9) {
//            delta = 0x06;

        emit("// lo nibble", "");
        emit("lsls ", "r1, r_af, #20");
        emit("lsrs ", "r1, #28");
        emit("lsrs ", "r2, r_af, #HF_INDEX+1");
        emit("bcs ", "1f");
        emit("cmp ", "r1, #10");
        emit("blt ", "3f");
        emit("1:");
        emit("adds ", "r0, #0x06");

//        if ((f & HF) || (a & 0xf) > 9) {
//            delta = 0x06;
//            if (f&NF) {
//                if (0x10 & ((a&0xf)-delta)) newflags |= HF;
//            } else {
//                if (0x10 & ((a&0xf)+delta)) newflags |= HF;
//            }
//        }

        emit("cmp ", "r_temp, #NF");
        emit("blt ", "1f");
        emit("subs ", "r1, r0");
        emit("b   ", "2f");
        emit("1:");
        emit("adds ", "r1, r0");
        emit("2:");
        emit("lsls ", "r1, #28");
        emit("bcc ", "3f");
        emit("movs ", "r2, #HF");
        emit("orrs ", "r_temp, r2");
        emit("3:");

//        if (a > 0x99) {
//            newflags |= CF;
//            delta |= 0x60;
//        } else if (f&CF) {
//            delta |= 0x60;
//        }

        emit("// hi nibble", "");
        emit("movs ", "r2, #CF");
        emit("lsrs ", "r1, r_af, #8");
        emit("cmp ", "r1, #0x9a");
        emit("blt ", "1f");
        emit("orrs ", "r_temp, r2");
        emit("b   ", "2f");
        emit("1:");
        emit("ands ", "r2, r_af");
        emit("beq ", "3f");
        emit("2:");
        emit("adds ", "r0, #0x60");
        emit("3:");

//        if( f & NF) {
//            a = (a-delta)&0xff;
//        } else {
//            a = (a+delta)&0xff;
//        }
//        f = log_f[a] | newflags;

        emit("lsrs ", "r2, r_af, #NF_INDEX+1");
        emit("bcc ", "1f");
        emit("subs ", "r1, r0");
        emit("b   ", "2f");
        emit("1:");
        emit("adds ", "r1, r0");
        emit("2:");
        emit("uxtb", "r_af, r1");

        emit("ldr ", "r2, =_log_f");
        emit("ldrb", "r0, [r2, r_af]");

        emit("lsls ", "r_af, #8");
        emit("orrs ", "r_af, r0");
        emit("orrs ", "r_af, r_temp");
    }

    void Ope57() { // ld a,i
        i.emit_read_to_r0();
        a = ZeroExtendedByteInR0();
        set_logic_flags_preserve_reset(a, CF, PV);
        t++;
//        if (iff1 && (t+10 < frame_tacts)) f |= PV;
        iff1.emit_read_to_r0();
        emit("cmp ", "r0, #0");
        emit("beq ", "1f");
        emit("#ifndef USE_Z80_ARM_OFFSET_T");
        emit("ldr ", "r0, frame_tacts");
        emit("#else");
        emit("movs ", "r0, #0");
        emit("#endif");
        emit("subs ", "r0, #10");
        emit("cmp ", "r_t, r0");
        emit("bge ", "1f");
        emit("adds ", "r_af, #PV");
        emit("1:");
    }

    void Ope5F() { // ld a,r
        emit("#ifdef NO_UPDATE_RLOW_IN_FETCH");
        // save on counting r_low
        emit("lsrs ", "r0, r_t, #2");
        emit("#else");
        r_low.emit_read_to_r0();
        emit("#endif");
        emit("movs ", "r1, #0x7f");
        emit("ands ", "r0, r1");
        r_hi.emit_read_to_r1();
        emit("orrs ", "r0, r1");
        emit_reg8_from_r0(a);

//        f = (log_f[a] | (f & CF)) & ~PV;
        set_logic_flags_preserve_reset(a, CF, PV);
        t++;
//        if (iff2 && ((t+10 < frame_tacts) || eipos+8==t)) f |= PV;
        iff1.emit_read_to_r0();
        emit("cmp ", "r0, #0");
        emit("beq ", "1f");
        emit("#ifndef USE_Z80_ARM_OFFSET_T");
        emit("ldr ", "r0, frame_tacts");
        emit("#else");
        emit("movs ", "r0, #0");
        emit("#endif");
        emit("subs ", "r0, #10");
        emit("cmp ", "r_t, r0");
        emit("blt ", "2f");
        eipos.emit_read_to_r0();
        emit("adds ", "r0, #8");
        // don't need offset_t check for eipos, since it is also offset
        emit("cmp ", "r0, r_t");
        emit("bne ", "1f");
        emit("2:");
        emit("adds ", "r_af, #PV");
        emit("1:");
    }

    void Ope6F() { // rld
        temp8 tmp = Read(hl);
        memptr = hl+1;
//        Write(hl, (a & 0x0F) | (tmp << 4));
        emit_r0_from_reg8(a);
        emit("movs ", "r2, #0xf");
        emit("ands ", "r0, r2");
        emit_r1_from_reg_lo(tmp);
        emit("lsls ", "r1, #4");
        emit("orrs ", "r0, r1");
        Write(hl, LoByteAndGarbageInR0());
//        a = (a & 0xF0) | (tmp >> 4);
        emit_r0_from_reg8(a);
        emit("movs ", "r2, #0xf0");
        emit("ands ", "r0, r2");
        emit_r1_from_reg_lo(tmp);
        emit("lsrs ", "r1, #4");
        emit("orrs ", "r0, r1");
        a = ZeroExtendedByteInR0();
        set_logic_flags_preserve(a, CF);
        t += 10;
    }

    void Ope67() { // rrd
        temp8 tmp = Read(hl);
        memptr = hl+1;
//        Write(hl, (a << 4) | (tmp >> 4));
        emit_r0_from_reg8(a);
        emit("lsls ", "r0, #4");
        emit_r1_from_reg_lo(tmp);
        emit("lsrs ", "r1, #4");
        emit("orrs ", "r0, r1");
        Write(hl, LoByteAndGarbageInR0());
//        a = (a & 0xF0) | (tmp & 0x0F);
        emit_r0_from_reg8(a);
        emit("movs ", "r2, #0xf0");
        emit("ands ", "r0, r2");
        emit_r1_from_reg_lo(tmp);
        emit("bics ", "r1, r2");
        emit("orrs ", "r0, r1");
        a = ZeroExtendedByteInR0();
        set_logic_flags_preserve(a, CF);
        t += 10;
    }

    void Op76() { // halt
        halted = 1;
//        unsigned int st = (frame_tacts - t - 1) / 4 + 1;
//        t += 4 * st;
        emit("#ifndef USE_Z80_ARM_OFFSET_T");
        emit("ldr ", "r0, frame_tacts");
        emit("#else");
        emit("movs ", "r0, #0");
        emit("#endif");
        emit("subs ", "r0, r_t");
        emit("movs ", "r1, #3");
        emit("adds ", "r0, r1");
        emit("bics ", "r0, r1");
        emit("add ", "r_t, r0");
#ifndef NO_USE_REPLAY
        if(handler.io) // replay is active
        {
            r_low += fetches;
            fetches = 0;
        }
        else
            r_low += st;
#else
        emit("#ifndef NO_UPDATE_RLOW_IN_FETCH");
        r_low.emit_read_to_r1();
        emit("lsrs ", "r0, #2");
        emit("add ", "r0, r1");
        r_low.emit_write_from_r0();
        emit("#endif");
#endif
    }

    void Ope44() { // neg
        // there are multiple neg op codes, so call (well bx) the function
        emit_call_func("neg8");
    }

#define HELPER_START -1
    #define HELPER_END -2
    void emit_helper_functions(const char *prefix, int num) {
        char buf[128];
        reset_function_state();
        if (!strcmp(prefix, "ope")) {
            if (num == 0xb0) {
                emit("ldX_common:");
                t += 8;
                temp8 tempbyte = Read(hl);
                Write(de, tempbyte);
//        tempbyte += a; tempbyte = (tempbyte & F3) + ((tempbyte << 4) & F5);
//        f = (f & ~(NF|HF|PV|F3|F5)) + tempbyte;
                emit("preserve_only_flags", "r1, (CF|SF|ZF)");
                emit("lsrs ", "r0, r_af, #8");
                emit("add ", "r_temp, r0");
                emit_call_func("set_af35_special_r_temp");

                --bc;
                // if (bc) {
                emit("beq ", "6f");
                //      f |= PV;
                emit("adds ", "r_af, #PV");
                // }
                emit("6:");
                emit_function_return();

                reset_function_state();
                emit("cpX_common:");
                emit_save_lr_if_not_done_already(); // save this now so our stack doesn't get tangled
                /**
    t += 8;
    byte cf = f & CF;
    byte tempbyte = Read(hl++);
    f = cpf8b[a*0x100 + tempbyte] + cf;

        NOTE
            byte tempbyte = (i >> 8) - (i & 0xFF) - ((_sbcf[i] & HF) >> 4);
		    _cpf8b[i] = (_sbcf[i] & ~(F3|F5|PV|CF)) + (tempbyte & F3) + ((tempbyte << 4) & F5);

    if (--bc & 0xFFFF) f |= PV; //???
    memptr++;
                 */
                t += 8;
                emit("push", "{r_af}");
                Read(hl);
                emit("mov ", "r_temp, r0");
                emit_call_func("cp8");
                emit("lsrs ", "r0, r_af, #8");
                emit("subs ", "r0, r_temp");
                emit("lsrs ", "r1, r_af, #HF_INDEX+1");
                emit("bcc 6f");
                emit("subs ", "r0, #1");
                emit("6:");
                emit("mov ", "r_temp, r0");
                emit("preserve_only_flags", "r1, (SF|HF|NF|ZF)");
                emit_call_func("set_af35_special_r_temp");
                emit("pop ", "{r0}");
                emit("movs ", "r1, #CF");
                emit("ands ", "r0, r1");
                emit("orrs ", "r_af, r0");

                --bc;
                // if (bc) {
                emit("beq ", "6f");
                //      f |= PV;
                emit("adds ", "r_af, #PV");
                // }
                emit("6:");
                memptr++;
                emit_function_return();
            }
        } else if (!strcmp(prefix, "opli_r35_")) {
            if (num == HELPER_START) {
                // note that mem_l is a marker
                Reg8Ref arg[] = { b, c, d, e, h, l, mem_l, a};

#ifndef USE_LARGER_FASTER_CB
                // we start here as it is right at the end of opl_table and it has too be close
                emit("opcbbitX_table:");
                for(int i=0;i<count_of(arg);i++) {
                    snprintf(buf, sizeof(buf), ".short opcbbit%d + 1 - opcbbitX_table", i);
                    emit(buf);
                }
                emit("");
#endif
                emit("opddcb_bitX_table:");
                for(uint i=0;i<count_of(arg);i++) {
                    snprintf(buf, sizeof(buf), ".short opddcb_bit + 1 - opddcb_bitX_table");
                    emit(buf);
                }
                emit("");
#ifndef USE_LARGER_FASTER_CB
                emit("opcbX_table:");
                for(int i=0;i<count_of(arg);i++) {
                    snprintf(buf, sizeof(buf), ".short opcb%d + 1 - opcbX_table", i);
                    emit(buf);
                }
                emit("");
#endif
                emit("opddcb_X_table:");
                for(uint i=0;i<count_of(arg);i++) {
                    snprintf(buf, sizeof(buf), ".short opddcb_%d + 1 - opddcb_X_table", i);
                    emit(buf);
                }
                emit("");
                for (uint i = 0; i < count_of(arg); i++) {
#ifndef USE_LARGER_FASTER_CB
                    reset_function_state();
                    for(int bit = 0; bit<2; bit++) {
                        reset_function_state();
                        snprintf(buf, sizeof(buf), "opcb%s%d:", bit?"bit":"", i);
                        emit(buf);
                        if (arg[i].reg != MEMPTR) {
                            emit_r_temp_from_reg8(arg[i]);
                            emit_save_lr_if_not_done_already();
                            emit("blx ", "r2");
                            emit_reg8_from_r_temp(arg[i]);
                            emit_function_return();
                        } else {
                            temp8 v = Read(hl); // note even though this isn't referenced in first path, it is necessary (because it has a side effect in codegen)
                            if (bit) {
                                // e.g.
                                //bitmem(Read(hl), 3);
                                //t += 4;
                                t += 4;
                                emit("bx ", "r2");
                                emit("");
                            } else {
                                // e.g.
                                // temp8 v = Read(hl); res(v, 6); Write(hl, v);
                                // t += 7;
                                emit_save_lr_if_not_done_already();
                                emit("blx ", "r2");
                                Write(hl, v);
                                t += 7;
                                emit_function_return();
                            }
                        }
                    }

#endif
                    reset_function_state();
                    snprintf(buf, sizeof(buf), "opddcb_%d:", i);
                    emit(buf);
                    //dword ptr; // pointer to DDCB operand
                    // ptr = ((op1 == 0xDD) ? ix : iy) + (signed char)ReadInc(pc);
                    // memptr = ptr;
                    temp8 v = Read(memptr);
                    // e.g.
                    // t += 4;
                    // byte v = (this->*logic_ix_opcodes[opcode])(Read(ptr));
                    // // select destination register for shift/res/set
                    // (this->*reg_offset[opcode & 7]) = v; // ???
                    // Write(ptr, v);
                    // t += 11;

                    emit_save_lr_if_not_done_already();
                    emit("blx ", "r2");
                    if (arg[i].reg != MEMPTR) {
                        emit_reg8_from_r_temp(arg[i]);
                    }
                    Write(memptr, v);
                    t += 11 + 4; // 4 from fetch earlier
                    emit_function_return();
                }
                reset_function_state();
                emit("opddcb_bit:");
                // use a common bitm variant as it isn't dependent on which register
                // e.g.
                // t += 4;
                // byte v = (this->*logic_ix_opcodes[opcode])(Read(ptr));
                // t += 8;
                // select destination register for shift/res/set
                // all the ddcb bit functions use memptr for r3,r5 and don't store the result
                t += 8 + 4; // 4 from fetch earlier
                temp8 __unused v = Read(memptr); // required because it is used in r_temp in the called function
                emit("bx ", "r2");
                emit("");
            }
        } else if (!strcmp(prefix, "op")) {
            if (num == HELPER_START) {
                // Define the interrupt function... simple enough to do here, so no need to hard code
                emit(".thumb_func");
                reset_function_state();
                emit("z80a_interrupt:");
                temp16 __unused intad = 0x38; // this has side effects in generated code
//                byte vector = 0xff;
//                if(im >= 2) // im2
//                {
//                    word vec = vector + i*0x100;
//                    intad = Read(vec) + 0x100*Read(vec+1);
//                    t += 19;
//                } else {
//                    t += 13;
//                }

                // need lr push in both codepaths
                emit_save_lr_if_not_done_already();
                im.emit_read_to_r0();
                emit("cmp ", "r0, #2");
                emit("blt ", "1f");
                i.emit_read_to_r0();
                emit("lsls ", "r0, #8");
                emit("movs ", "r1, #0xff");
                emit("orrs ", "r0, r1");
                intad = Read2(WordInR0());
                emit("adds ", "r_t, #6");
                emit("1:");
                emit("adds ", "r_t, #13");

                push(pc);
                pc = intad;
                memptr = intad;
                halted = 0;
                iff1 = iff2 = 0;
                r_low = ((WordInR0)r_low) + 1;
                emit_function_return();
            }
        }
    }

    void OpeA0() { // ldi
        emit_call_func("ldX_common");
        ++hl;
        ++de;
    }

    void OpeA1() { // cpi
        emit_call_func("cpX_common");
        ++hl;
    }

    void OpeA8() { // ldd
        emit_call_func("ldX_common");
        --hl;
        --de;
    }

    void OpeA9() { // cpd
        emit_call_func("cpX_common");
        --hl;
    }

	DECLARE_REG16(PC, pc, pc_l, pc_h)
	DECLARE_REG16(SP, sp, sp_l, sp_h)
    rs_var(r_hi);
	rs_var(iff1);
	rs_var(iff2);
	rs_var(halted);
	rs_var(r_low);
	rs_var(i);
	rs_var(eipos);
	rs_var(im);
	/**
	 * r0 -
	 * r1
	 * r2 - iy
	 * r3 - temp?
	 * r4 - pc
	 * r5 - sp
	 * r6 - t
	 * r7 - af
	 * r8 - bc
	 * r9 - de
	 * r10 - hl
	 * r11 - ix
	 * r12 - memptr
	 */
	DECLARE_REG16(MEMPTR, memptr, mem_l, mem_h)	// undocumented register
	DECLARE_REG16(IX, ix, xl, xh)
	DECLARE_REG16(IY, iy, yl, yh)

	DECLARE_REG16(BC, bc, c, b)
	DECLARE_REG16(DE, de, e, d)
	DECLARE_REG16(HL, hl, l, h)
	DECLARE_REG16(AF, af, f, a)
	DECLARE_REG16(TEMPORARY, temporary, temp_lo, temp_hi)
	DECLARE_REG16(TEMPORARY_RESTRICTED, restricted, restriced_lo, restricted_hi)
	struct alt {
        rs_var(bc);
        rs_var(de);
        rs_var(hl);
        rs_var(af);
	} alt;
    rs_var(scratch); // another scratch register
};

}//namespace xZ80

#endif//__Z80_H__
