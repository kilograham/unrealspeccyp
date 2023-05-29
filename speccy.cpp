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

#include "std.h"

#include "speccy.h"
#include "devices/device.h"
#include "z80/z80.h"
#include "devices/memory.h"
#include "devices/ula.h"
#include "devices/input/keyboard.h"
#ifndef NO_USE_KEMPSTON
#include "devices/input/kempston_joy.h"
#ifndef USE_MU
#include "devices/input/kempston_mouse.h"
#endif
#endif
#ifndef NO_USE_TAPE
#include "devices/input/tape.h"
#endif
#ifndef NO_USE_BEEPER
#include "devices/sound/beeper.h"
#endif
#ifndef NO_USE_AY
#include "devices/sound/ay.h"
#endif
#ifndef NO_USE_FDD
#include "devices/fdd/wd1793.h"
#endif
#include "tools/profiler.h"

PROFILER_DECLARE(dev_e);
PROFILER_DECLARE(dev);
PROFILER_DECLARE(dev_s);
PROFILER_DECLARE(frame);

//=============================================================================
//	eSpeccy::eSpeccy
//-----------------------------------------------------------------------------
eSpeccy::eSpeccy() : cpu(NULL), memory(NULL), frame_tacts(0)
	, int_len(0), nmi_pending(0), t_states(0)
{
	// pentagon timings
	frame_tacts = 71680;
	int_len = 32;

	memory = new eMemory;
	devices.Add(new eRom(memory));
	devices.Add(new eRam(memory));
	devices.Add(new eUla(memory));
	devices.Add(new eKeyboard);
#ifndef NO_USE_KEMPSTON
	devices.Add(new eKempstonJoy);
#ifndef USE_MU
	devices.Add(new eKempstonMouse);
#endif
#endif
#ifndef NO_USE_BEEPER
	devices.Add(new eBeeper);
#endif
#ifndef NO_USE_AY
	devices.Add(new eAY);
#endif
#ifndef NO_USE_FDD
	devices.Add(new eWD1793(this, Device<eRom>()));
#endif
#ifndef NO_USE_TAPE
	devices.Add(new eTape(this));
#endif
	cpu = new xZ80::eZ80(memory, &devices, frame_tacts);
	Reset();
}
//=============================================================================
//	eSpeccy::~eSpeccy
//-----------------------------------------------------------------------------
#ifndef NO_USE_DESTRUCTORS
eSpeccy::~eSpeccy()
{
	delete cpu;
	delete memory;
}
#endif
//=============================================================================
//	eSpeccy::Reset
//-----------------------------------------------------------------------------
void eSpeccy::Reset()
{
	cpu->Reset();
	devices.Init();
	devices.Reset();
}
#ifndef NO_USE_128K
//=============================================================================
//	eSpeccy::Mode48k
//-----------------------------------------------------------------------------
bool eSpeccy::Mode48k() const
{
	return Device<eRam>()->Mode48k();
}
//=============================================================================
//	eSpeccy::Mode48k
//-----------------------------------------------------------------------------
void eSpeccy::Mode48k(bool on)
{
	Device<eRom>()->Mode48k(on);
	Device<eRam>()->Mode48k(on);
	Device<eUla>()->Mode48k(on);
}
#endif
//=============================================================================
//	eSpeccy::Update
//-----------------------------------------------------------------------------
void eSpeccy::Update(int* fetches)
{
	{
		PROFILER_SECTION(dev_s);
		devices.FrameStart(fetches ? 0 : cpu->T());
	}
	{
		PROFILER_SECTION(frame);
#ifndef NO_USE_REPLAY
		if(fetches)
			cpu->Replay(*fetches);
		else
			cpu->Update(int_len, &nmi_pending);
#else
		assert(fetches == nullptr);
		cpu->Update(int_len, &nmi_pending);
#endif
	}
	{
		PROFILER_SECTION(dev);
		devices.FrameUpdate();
	}
	{
		PROFILER_SECTION(dev_e);
		devices.FrameEnd(fetches ? cpu->T() : cpu->FrameTacts() + cpu->T());
	}
	t_states += fetches ? cpu->T() : cpu->FrameTacts();
}
