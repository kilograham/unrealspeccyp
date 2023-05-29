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

#ifndef	__Z80_H__
#define	__Z80_H__

#include "../platform/endian.h"
#ifdef USE_Z80_ARM
#include "z80arm.h"
#else

#include "z80_op_tables.h"

#pragma once

class eMemory;
class eRom;
class eUla;
class eDevices;

namespace xZ80
{
#ifdef USE_BIG_ENDIAN
#define DECLARE_REG16(reg, low, high)\
union\
{\
	struct\
	{\
		byte reg##xx;\
		byte reg##x;\
		byte high;\
		byte low;\
	};\
	int reg;\
};
#else//USE_BIG_ENDIAN
#define DECLARE_REG16(reg, low, high)\
union\
{\
	struct\
	{\
		byte low;\
		byte high;\
		byte reg##x;\
		byte reg##xx;\
	};\
	int reg;\
};
#endif//USE_BIG_ENDIAN

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


//*****************************************************************************
//	eZ80
//-----------------------------------------------------------------------------
class eZ80
{
public:
	eZ80(eMemory* m, eDevices* d, dword frame_tacts = 0);
	void Reset();
	void Update(int int_len, int* nmi_pending);
#ifndef NO_USE_REPLAY
	void Replay(int fetches);
#endif

	dword FrameTacts() const { return frame_tacts; }
	dword T() const { return t; }

	class eHandlerIo
	{
	public:
		virtual byte Z80_IoRead(word port, int tact) = 0;
	};
	void HandlerIo(eHandlerIo* h) { handler.io = h; }
	eHandlerIo* HandlerIo() const { return handler.io; }

	class eHandlerStep
	{
	public:
		virtual void Z80_Step(eZ80* z80) = 0;
	};
#ifndef NO_USE_FAST_TAPE
	void HandlerStep(eHandlerStep* h) { handler.step = h; }
	eHandlerStep* HandlerStep() const { return handler.step; }
#endif

//protected:
	void Int();
	void Nmi();
	void Step();
	void StepF();
	byte Fetch()
	{
#ifndef NO_USE_REPLAY
		--fetches;
#endif
#ifndef NO_UPDATE_RLOW_IN_FETCH
		++r_low;// = (cpu->r & 0x80) + ((cpu->r+1) & 0x7F);
#endif
		t += 4;
		return ReadInc(pc);
	}

	typedef byte temp8;
	typedef byte temp8_xy;
	typedef int temp16;
	typedef unsigned temp16_2;
	typedef unsigned temp16_xy;
	typedef unsigned scratch16;
	typedef signed char address_delta;

	byte IoRead(word port) const;
	void IoWrite(word port, byte v);
	byte Read(word addr) const;
	byte ReadInc(int& addr) const;
	int Read2(word addr) const;
	int Read2Inc(int& addr) const;
	void Write(word addr, byte v);
	inline void WriteXY(word addr, byte v) { Write(addr, v); }
	void Write2(word addr, int v);

	typedef void (eZ80::*CALLFUNC)();
	typedef byte (eZ80::*CALLFUNCI)(byte);

public:
	#include "z80_op.h"
	#include "z80_op_noprefix.h"
	#include "z80_op_cb.h"
	#include "z80_op_dd.h"
	#include "z80_op_ed.h"
	#include "z80_op_fd.h"
	#include "z80_op_ddcb.h"

	void InitOpNoPrefix();
	void InitOpCB();
	void InitOpDD();
	void InitOpED();
	void InitOpFD();
	void InitOpDDCB();

#ifndef NDEBUG
	void    set_breakpoint(int bp_addr);
#endif
	protected:
	eMemory*	memory;
	eRom*		rom;
	eUla*		ula;
	eDevices*	devices;

	// z80arm codegen needs to capture the if/else
	template <typename A, typename B> void if_nonzero(int v, A&& a, B&& b) {
		if (v) a(); else b();
	}
	template <typename A> void if_nonzero(int v, A&& a) {
		if (v) a();
	}

	template <typename A> void if_equal(int v, int cmp, A&& a) {
		if (v == cmp) a();
	}

	template <typename A> void if_zero(int v, A&& a) {
		if (!v) a();
	}
		// z80arm codegen needs to capture the if/else
	template <typename A, typename B> void if_flag_set(byte flag, A&& a, B&& b) {
		if (f&flag) a(); else b();
	}

	template <typename A> void if_flag_set(byte flag, A&& a) {
		if (f&flag) a();
	}

	// z80arm codegen needs to capture the if/else
	template <typename A, typename B> void if_flag_clear(byte flag, A&& a, B&& b) {
		if (!(f&flag)) a(); else b();
	}
	template <typename A> void if_flag_clear(byte flag, A&& a) {
		if (!(f&flag)) a();
	}

	inline void set_a35_flags_preserve_set(byte preserve, byte set) {
		f = (f & (preserve)) | (a & (F3|F5)) | set;
	}

	inline void set_logic_flags_preserve(byte value, byte preserve_flags) {
		set_logic_flags_preserve_reset(value, preserve_flags, 0);
	}

	inline void set_logic_flags_preserve_reset(byte value, byte preserve_flags, byte reset_flags) {
		f = log_f[value] | (f & preserve_flags);
		if (reset_flags) {
			f &= ~reset_flags;
		}
	}

	struct eHandler
	{
		eHandler() : io(NULL)
#ifndef NO_USE_FAST_TAPE
		,step(NULL)
#endif
		{}
		eHandlerIo*	io;
#ifndef NO_USE_FAST_TAPE
		eHandlerStep* step;
#endif
	};
	eHandler handler;

	int		t;
	int		im;
	int		eipos;
	int		frame_tacts; 	// t-states per frame
#ifndef NO_USE_REPLAY
	int		fetches;		// .rzx replay fetches
#endif
#ifndef NDEBUG
	int     bp_addr;
	int     last_pc;
	void    breakpoint_hit();
#endif

	DECLARE_REG16(pc, pc_l, pc_h)
	DECLARE_REG16(sp, sp_l, sp_h)
	DECLARE_REG16(ir, r_low, i)
	union
	{
		struct
		{
			byte r_hi;
			byte iff1;
			byte iff2;
			byte halted;
		};
		dword int_flags;
	};
	DECLARE_REG16(memptr, mem_l, mem_h)	// undocumented register
	DECLARE_REG16(ix, xl, xh)
	DECLARE_REG16(iy, yl, yh)

	DECLARE_REG16(bc, c, b)
	DECLARE_REG16(de, e, d)
	DECLARE_REG16(hl, l, h)
	DECLARE_REG16(af, f, a)
	struct eAlt
	{
		DECLARE_REG16(bc, c, b)
		DECLARE_REG16(de, e, d)
		DECLARE_REG16(hl, l, h)
		DECLARE_REG16(af, f, a)
	} alt;

	CALLFUNC normal_opcodes[0x100];
	CALLFUNC logic_opcodes[0x100];
	CALLFUNC ix_opcodes[0x100];
	CALLFUNC iy_opcodes[0x100];
	CALLFUNC ext_opcodes[0x100];
	CALLFUNCI logic_ix_opcodes[0x100];

	typedef byte (eZ80::*REGP);
	REGP reg_offset[8];
	byte reg_unused;

	inline word get_caller_pc() const { return pc; }
	inline void set_caller_pc(word v) { pc = v; }
	inline byte get_caller_a() const { return a; }
	inline void set_caller_a(byte v) { a = v; }
	inline void set_caller_flag(byte flags) { f |= flags; }
	inline byte get_caller_b() const { return b; }
	inline void set_caller_b(byte v) { b = v; }
	inline byte get_caller_c() const { return c; }
	inline void set_caller_bc(word v) { bc = v; }
	inline void set_caller_h(byte v) { h = v; }
	inline byte get_caller_l() const { return l; }
	inline void set_caller_l(byte v) { l = v; }
	inline word get_caller_ix() const { return ix; }
	inline void set_caller_ix(word v) { ix = v; }
	inline word get_caller_de() const { return de; }
	inline void set_caller_de(word v) { de = v; }
	inline void delta_caller_t(int delta) { t += delta; }
};

}//namespace xZ80
#endif

#endif//__Z80_H__
