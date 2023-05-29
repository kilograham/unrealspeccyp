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

#ifdef USE_Z80_ARM

#ifdef USE_BANKED_MEMORY_ACCESS
// we want somewhere we can dump writes to ROM
#include "hardware/structs/xip_ctrl.h"
#endif
#include "../std.h"
#include "../devices/memory.h"
#include "../devices/ula.h"
#include "../devices/device.h"

#include "z80arm.h"
#include "../platform/platform.h"
#include "../speccy.h"

#include "z80khan.h"
#include "hardware/interp.h"

namespace xZ80
{

//=============================================================================
//	eZ80::eZ80
//-----------------------------------------------------------------------------
    eZ80::eZ80(eMemory* _m, eDevices* _d, dword _frame_tacts)
        : memory(_m), rom(_d->Get<eRom>()), ula(_d->Get<eUla>()), devices(_d)
        , frame_tacts(_frame_tacts)
#ifndef NO_USE_REPLAY
        fetches(0)
#endif
    {
        // todo eipos
        __builtin_memset(&z80a_resting_state, 0, sizeof(z80a_resting_state));
#ifndef USE_BANKED_MEMORY_ACCESS
        z80a_resting_state.memory_64k = memory->Get(eRom::ROM_48);
#endif
#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
        z80a_resting_state.bp_addr = -1;
#endif
#endif
    }

#ifdef USE_BANKED_MEMORY_ACCESS
    void eZ80::InitMemoryBanks()
    {
        uint8_t **bank_writes = memory->GetBankWrites();
#if !PICO_ON_DEVICE
        bank_writes[0] = 0;
#else
        z80a_resting_state.interp = interp0;
        hw_clear_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_ERR_BADWRITE_BITS);
        auto bit_dumpster = (uint8_t *)XIP_NOCACHE_NOALLOC_BASE;
        bank_writes[0] = bit_dumpster;

        interp0->base[0] = (uintptr_t)memory->GetBankReads();
        interp0->base[1] = 0;
        interp1->base[0] = (uintptr_t)bank_writes;
        interp1->base[1] = 0;

#ifndef NDEBUG
        printf("%p %p\n", memory->GetBankReads(), bank_writes);
        printf("%p %p\n", memory->GetBankReads()[0], bank_writes[0]);
        printf("%p %p\n", memory->GetBankReads()[1], bank_writes[1]);
        printf("%p %p\n", memory->GetBankReads()[2], bank_writes[2]);
        printf("%p %p\n", memory->GetBankReads()[3], bank_writes[3]);
#endif
        interp_config c = interp_default_config();
        interp_config_set_shift(&c, 12);
        interp_config_set_mask(&c, 2, 3);
        interp_set_config(interp0, 0, &c);
        interp_set_config(interp1, 0, &c);
#endif
    }
#endif
//=============================================================================
//	eZ80::Reset
//-----------------------------------------------------------------------------
    void eZ80::Reset()
    {
        assert(!z80a_resting_state.active);
#ifndef NO_USE_FAST_TAPE
        HandlerStep(NULL);
#endif
#ifndef USE_HACKED_DEVICE_ABSTRACTION
        handler.io = NULL;
#endif
        z80a_reset();
    }
    void eZ80::Update(int int_len, int* nmi_pending)
    {
        assert(!z80a_resting_state.active);
        //assert(!nmi_pending);
        z80a_update(int_len);
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
////=============================================================================
////	eZ80::Nmi
////-----------------------------------------------------------------------------
//    void eZ80::Nmi()
//    {
//#if 1
//        assert(false);
//#else
//        push(pc);
//        pc = 0x66;
//        iff1 = halted = 0;
//#endif
//    }

void z80a_step_hook_func() {
#ifndef NO_USE_FAST_TAPE
    static eZ80 *z80;
    if (!z80) {
        z80 = xPlatform::Handler()->Speccy()->CPU();
    }
    assert(z80->HandlerStep());
    z80->HandlerStep()->Z80_Step(z80);
#endif
}

#ifndef NDEBUG
#ifdef ENABLE_BREAKPOINT_IN_DEBUG
    void eZ80::set_breakpoint(int pc_addr) {
        assert(!z80a_resting_state.active);
        z80a_resting_state.bp_addr = pc_addr;
    }
#endif
#endif
}//namespace xZ80

#ifndef NDEBUG
void z80a_breakpoint_hit() {
    struct _z80a_resting_state *s = &z80a_resting_state;
    static int i=0;
    printf("%d %d %04x %04x\n", i++, (int)s->t, s->hl, s->af);
}
#endif
#endif
