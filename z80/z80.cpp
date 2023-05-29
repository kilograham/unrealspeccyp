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

#include "z80.h"

namespace xZ80
{

//=============================================================================
//	eZ80::eZ80
//-----------------------------------------------------------------------------
eZ80::eZ80(eMemory* _m, eDevices* _d, dword _frame_tacts)
	: memory(_m), rom(_d->Get<eRom>()), ula(_d->Get<eUla>()), devices(_d)
	, t(0), im(0), eipos(0)
	, frame_tacts(_frame_tacts),
#ifndef NO_USE_REPLAY
	fetches(0),
#endif
	reg_unused(0)
{
	pc = sp = ir = memptr = ix = iy = 0;
	bc = de = hl = af = alt.bc = alt.de = alt.hl = alt.af = 0;
	int_flags = 0;
#ifndef NDEBUG
	bp_addr = -1;
#endif

	InitOpNoPrefix();
	InitOpCB();
	InitOpDD();
	InitOpED();
	InitOpFD();
	InitOpDDCB();

	// offsets to b,c,d,e,h,l,<unused>,a  from cpu.c
	const REGP r_offset[] =
	{
		&eZ80::b, &eZ80::c, &eZ80::d, &eZ80::e,
		&eZ80::h, &eZ80::l, &eZ80::reg_unused, &eZ80::a
	};
	memcpy(reg_offset, r_offset, sizeof(r_offset));
}
//=============================================================================
//	eZ80::Reset
//-----------------------------------------------------------------------------
void eZ80::Reset()
{
#ifndef NO_USE_FAST_TAPE
	handler.step = NULL;
#endif
	handler.io = NULL;
	int_flags = 0;
	ir = 0;
	im = 0;
	pc = 0;
}
//=============================================================================
//	eZ80::Read
//-----------------------------------------------------------------------------
inline byte eZ80::Read(word addr) const
{
	return memory->Read(addr);
}

inline byte eZ80::ReadInc(int& addr) const
{
	return memory->Read(addr++);
}

inline int eZ80::Read2(word addr) const
{
	unsigned r = memory->Read(addr);
	r += memory->Read(addr+1) << 8u;
	return r;
}

inline int eZ80::Read2Inc(int& addr) const
{
	unsigned r = memory->Read(addr++);
	r += memory->Read(addr++) << 8u;
	return r;
}


//=============================================================================
//	eZ80::StepF
//-----------------------------------------------------------------------------
void eZ80::StepF()
{
#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
		if (pc == bp_addr) {
			breakpoint_hit();
		}
		last_pc = pc;
#endif
#endif
	rom->Read(pc);
#ifndef NO_USE_FAST_TAPE
	SAFE_CALL(handler.step)->Z80_Step(this);
#endif
	(this->*normal_opcodes[Fetch()])();
}
//=============================================================================
//	eZ80::Step
//-----------------------------------------------------------------------------
void eZ80::Step()
{
#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
	if (pc == bp_addr) {
		breakpoint_hit();
	}
	last_pc = pc;
#endif
#endif
	rom->Read(pc);
	(this->*normal_opcodes[Fetch()])();
}
//=============================================================================
//	eZ80::Update
//-----------------------------------------------------------------------------
void eZ80::Update(int int_len, int* nmi_pending)
{
//#define NO_USE_INTERRUPTS
#ifndef NO_USE_INTERRUPTS
	if(!iff1 && halted)
		return;
	// INT check separated from main Z80 loop to improve emulation speed
	while(t < int_len)
	{
		if(iff1 && t != eipos) // int enabled in CPU not issued after EI
		{
			Int();
			break;
		}
		Step();
		if(halted)
			break;
	}
#endif
	eipos = -1;
#ifndef NO_USE_FAST_TAPE
	if(handler.step)
	{
		while(t < frame_tacts)
		{
			StepF();
		}
	}
	else
#endif
	{
		while(t < frame_tacts)
		{
			Step();
//			if(*nmi_pending)
//			{
//				--*nmi_pending;
//				if(pc >= 0x4000)
//				{
//					Nmi();
//					*nmi_pending = 0;
//				}
//			}
		}
	}
	t -= frame_tacts;
	eipos -= frame_tacts;
}
#ifndef NO_USE_REPLAY
//=============================================================================
//	eZ80::Replay
//-----------------------------------------------------------------------------
void eZ80::Replay(int _fetches)
{
	fetches = _fetches;
	t = 0;
	eipos = -1;
	while(fetches > 0)
	{
		Step();
	}
	if(iff1)
		Int();
	fetches = 0;
}
#endif
//=============================================================================
//	eZ80::Int
//-----------------------------------------------------------------------------
void eZ80::Int()
{
	byte vector = 0xff;
	word intad = 0x38;
	if(im >= 2) // im2
	{
		word vec = vector + i*0x100;
		intad = Read(vec) + 0x100*Read(vec+1);
	}
	t += (im < 2) ? 13 : 19;
	push(pc);
	pc = intad;
	memptr = intad;
	halted = 0;
	iff1 = iff2 = 0;
	++r_low;
}
//=============================================================================
//	eZ80::Nmi
//-----------------------------------------------------------------------------
void eZ80::Nmi()
{
	push(pc);
	pc = 0x66;
	iff1 = halted = 0;
}

#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
void eZ80::set_breakpoint(int _bp_addr) {
	bp_addr = _bp_addr;
}
void eZ80::breakpoint_hit() {
	static int i = 0;
	printf("%d %d %04x %04x\n", i++, t, hl, af);
}
#endif
#endif

}//namespace xZ80
