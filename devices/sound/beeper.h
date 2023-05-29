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

#ifndef __BEEPER_H__
#define __BEEPER_H__

#include "device_sound.h"

#pragma once

//=============================================================================
//	eBeeper
//-----------------------------------------------------------------------------
class eBeeper : public eDeviceSound
{
public:
	eBeeper();
	void IoWrite(word port, byte v, int tact) override;
	static eDeviceId Id() { return D_BEEPER; }
	void Reset() override;
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoWrite(word port) const;
	virtual dword IoNeed() const { return ION_WRITE; }
#endif
	void FrameStart(dword tacts) override;
	void FrameEnd(dword tacts) override;
};

#endif//__BEEPER_H__
