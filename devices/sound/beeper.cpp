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

#ifndef NO_USE_BEEPER
#include "../../std.h"
#include "beeper.h"
#include "khan_lib.h"
#include "../../options_common.h"

eBeeper::eBeeper() {
	// need this to create vtable
}
//=============================================================================
//	eBeeper::IoWrite
//-----------------------------------------------------------------------------
#ifndef USE_HACKED_DEVICE_ABSTRACTION
bool eBeeper::IoWrite(word port) const
{
	return !(port&1);
}
#endif
//=============================================================================
//	eBeeper::IoWrite
//-----------------------------------------------------------------------------
void eBeeper::IoWrite(word port, byte v, int tact)
{
//	const short spk_vol = 8192;
//	const short mic_vol = 1000;
//	short spk = (v & 0x10) ? spk_vol : 0;
//	short mic = (v & 0x08) ? mic_vol : 0;
//	short mono = spk + mic;
//	mono = (mono * 182 * xPlatform::OpSoundVolume()) >> 16; //(0->255)
	if (!(port&1)) {
		uint8_t mono = (((v & 0x10) ? 21 : 0) + ((v & 0x08) ? 4 : 0)) * xPlatform::OpSoundVolume();
		Update(tact, mono, mono);
	}
}

void eBeeper::Reset()
{
	khan_beeper_reset();
}
//=============================================================================
//	eDeviceSound::FrameStart
//-----------------------------------------------------------------------------
void eBeeper::FrameStart(dword tacts)
{
	static int frame_number = 0;
	khan_beeper_begin_frame(tacts, frame_number++);
}

//=============================================================================
//	eDeviceSound::FrameEnd
//-----------------------------------------------------------------------------
void eBeeper::FrameEnd(dword tacts)
{
	khan_beeper_end_frame(tacts);
}
#endif
