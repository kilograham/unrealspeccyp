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

#ifndef __KEMPSTON_JOY_H__
#define __KEMPSTON_JOY_H__

#include "../device.h"

#pragma once

#define KEMPSTON_R 0x01
#define KEMPSTON_L 0x02
#define KEMPSTON_D 0x04
#define KEMPSTON_U 0x08
#define KEMPSTON_F 0x10

class eKempstonJoy : public eDevice
{
public:
	virtual void Init();
	virtual void Reset();
#ifndef USE_MU
	virtual bool IoRead(word port) const;
#endif
	virtual void IoRead(word port, byte* v, int tact);

#ifndef USE_MU
	void OnKey(char key, bool down);
#else
	inline void setState(byte _state) {
	    state = _state;
	}
#endif
	static eDeviceId Id() { return D_KEMPSTON_JOY; }

#ifndef USE_MU
	virtual dword IoNeed() const { return ION_READ; }
#endif
protected:
#ifndef USE_MU
	void KeyState(char key, bool down);
#endif
	byte state;
};

#endif//__KEMPSTON_JOY_H__
