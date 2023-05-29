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

#ifndef __DEVICE_SOUND_H__
#define __DEVICE_SOUND_H__

#include "../device.h"

#pragma once

//union SNDSAMPLE
//{
//	dword sample; // left/right channels in low/high WORDs
//	struct { word left, right; } ch; // or left/right separately
//};
//
const dword SNDR_DEFAULT_SYSTICK_RATE = 71680 * 50; // ZX-Spectrum Z80 clock

#if defined(USE_MU) && PICO_ON_DEVICE
#if KHAN128_I2S
#define PICO_SAMPLE_RATE 24000 // some I2S chips seem unhappy with 22050
#else
// PWM must be 22058
#define PICO_SAMPLE_RATE 22058
#endif
#else
#define PICO_SAMPLE_RATE 44100
#endif

const dword SNDR_DEFAULT_SAMPLE_RATE = PICO_SAMPLE_RATE;

//=============================================================================
//	eDeviceSound
//-----------------------------------------------------------------------------
class eDeviceSound : public eDevice
{
public:
	eDeviceSound();
//	void SetTimings(dword clock_rate, dword sample_rate);

	virtual void FrameStart(dword tacts);
	virtual void FrameEnd(dword tacts);
	virtual void Update(dword tact, dword l, dword r);

protected:
	dword mix_l, mix_r;
//	dword clock_rate, sample_rate;

private:
	void Flush(int64_t endtick);
};

#endif//__DEVICE_SOUND_H__

