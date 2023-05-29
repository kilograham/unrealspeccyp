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

#ifndef NO_USE_KEMPSTON
#include "../../std.h"
#include "kempston_joy.h"

void eKempstonJoy::Init() { Reset(); }
void eKempstonJoy::Reset() { state = 0; }

#ifndef USE_MU
//=============================================================================
//	eKempstonJoy::IoRead
//-----------------------------------------------------------------------------
bool eKempstonJoy::IoRead(word port) const
{
	if(port&0x20)
		return false;
	// skip kempston mouse ports
	port |= 0xfa00; // A13,A15 not used in decoding
	if((port == 0xfadf || port == 0xfbdf || port == 0xffdf))
		return false;
	return true;
}
#endif
//=============================================================================
//	eKempstonJoy::IoRead
//-----------------------------------------------------------------------------
void eKempstonJoy::IoRead(word port, byte* v, int tact)
{
	*v = state;
}

struct eButton
{
	char key;
	byte bit;
};
#ifndef USE_MU
enum { BUTTONS_COUNT = 5 };
static const eButton buttons[BUTTONS_COUNT] =
{
	{'r',	KEMPSTON_R },
	{'l',	KEMPSTON_L },
	{'d',	KEMPSTON_D },
	{'u',   KEMPSTON_U },
	{'f',	KEMPSTON_F },
};
//=============================================================================
//	eKempstonJoy::OnKey
//-----------------------------------------------------------------------------
void eKempstonJoy::OnKey(char _key, bool _down)
{
	KeyState(_key, _down);
}
//=============================================================================
//	eKempstonJoy::KeyState
//-----------------------------------------------------------------------------
void eKempstonJoy::KeyState(char _key, bool _down)
{
	for(int i = 0; i < BUTTONS_COUNT; ++i)
	{
		const eButton& b = buttons[i];
		if(_key == b.key)
		{
			if(_down)
				state |= b.bit;
			else
				state &= ~b.bit;
			break;
		}
	}
}
#endif
#endif