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

#include "../../std.h"
#include "device_sound.h"
#include "../../platform/platform.h"
#include "../../speccy.h"

#include "khan_lib.h"

#if defined(NO_USE_BEEPER) && defined(NO_USE_AY)
#define NO_USE_SOUND
#endif

//=============================================================================
//	eDeviceSound::eDeviceSound
//-----------------------------------------------------------------------------
eDeviceSound::eDeviceSound() : mix_l(0), mix_r(0)
{
//	SetTimings(SNDR_DEFAULT_SYSTICK_RATE, SNDR_DEFAULT_SAMPLE_RATE);
}

//=============================================================================
//	eDeviceSound::FrameStart
//-----------------------------------------------------------------------------
void eDeviceSound::FrameStart(dword tacts)
{
}
//=============================================================================
//	eDeviceSound::Update
//-----------------------------------------------------------------------------
void eDeviceSound::Update(dword tact, dword l, dword r)
{
	if(!((l ^ mix_l) | (r ^ mix_r)))
		return;
	mix_l = l; mix_r = r;
	Flush(tact);
	// note we flush with the new level
}
//=============================================================================
//	eDeviceSound::FrameEnd
//-----------------------------------------------------------------------------
void eDeviceSound::FrameEnd(dword tacts)
{
//	dword endtick = (tacts * (qword)sample_rate * TICK_F) / clock_rate;
//	Flush(base_tick + endtick);
}

//=============================================================================
//	eDeviceSound::Flush
//-----------------------------------------------------------------------------
void eDeviceSound::Flush(int64_t endtick)
{
#ifndef NO_USE_SOUND
#ifndef NO_USE_BEEPER
	khan_beeper_level_change(endtick, (mix_l + mix_r)/2);
#endif
#endif
}