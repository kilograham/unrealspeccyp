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

#ifndef __AY_H__
#define __AY_H__

#ifndef NO_USE_AY
#include "device_sound.h"
#include "pico/audio.h"
#pragma once


extern struct audio_buffer_pool *producer_pool;

namespace xZ80 { class eZ80; }

const dword SNDR_DEFAULT_AY_RATE = 1774400; // original ZX-Spectrum soundchip clock fq

#pragma pack(push, 1)
struct AYREGS
{
	word fA, fB, fC;
	byte noise, mix;
	byte vA, vB, vC;
	word envT;
	byte env;
	byte portA, portB;
};
#pragma pack(pop)

// output volumes (#0000-#FFFF) for given envelope state or R8-R10 value
// AY chip has only 16 different volume values, so v[0]=v[1], v[2]=v[3], ...
struct SNDCHIP_VOLTAB
{
	dword v[32];
};

// generator's channel panning, % (0-100)
struct SNDCHIP_PANTAB
{
	dword raw[6];
	// structured as 'struct { dword left, right; } chan[3]';
};

// used as parameters to SNDCHIP::set_volumes(),
// if application don't want to override defaults
extern const SNDCHIP_VOLTAB* SNDR_VOL_AY;
extern const SNDCHIP_VOLTAB* SNDR_VOL_YM;

extern const SNDCHIP_PANTAB* SNDR_PAN_MONO;
extern const SNDCHIP_PANTAB* SNDR_PAN_ABC;
extern const SNDCHIP_PANTAB* SNDR_PAN_ACB;
extern const SNDCHIP_PANTAB* SNDR_PAN_BAC;
extern const SNDCHIP_PANTAB* SNDR_PAN_BCA;
extern const SNDCHIP_PANTAB* SNDR_PAN_CAB;
extern const SNDCHIP_PANTAB* SNDR_PAN_CBA;

//=============================================================================
//	eAY
//-----------------------------------------------------------------------------
class eAY : public eDeviceSound
{
	typedef eDeviceSound eInherited;
public:
	eAY();
#ifndef NO_USE_DESTRUCTORS
	virtual ~eAY() {}
#endif

#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoRead(word port) const;
	virtual bool IoWrite(word port) const;
	virtual void IoRead(word port, byte* v, int tact);
	virtual void IoWrite(word port, byte v, int tact);
#endif

	enum CHIP_TYPE { CHIP_AY, CHIP_YM, CHIP_MAX };
	static const char* GetChipName(CHIP_TYPE i);

	void SetChip(CHIP_TYPE type) { chiptype = type; }
	void SetTimings(dword system_clock_rate, dword chip_clock_rate, dword sample_rate);
	void SetVolumes(dword global_vol, const SNDCHIP_VOLTAB *voltab, const SNDCHIP_PANTAB *stereo);
	void SetRegs(const byte _reg[16]) { memcpy(reg, _reg, sizeof(reg)); ApplyRegs(0); }
	void Select(byte nreg);

	virtual void Reset() { _Reset(); }
	virtual void FrameStart(dword tacts);
	virtual void FrameEnd(dword tacts);

	static eDeviceId Id() { return D_AY; }
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual dword IoNeed() const { return ION_WRITE|ION_READ; }
#endif

	void Write(dword timestamp, byte val);
	byte Read();
protected:

private:
	dword t;
#ifndef USE_FAST_AY
	dword ta, tb, tc, tn, te;
	dword env;
	int denv;
	dword bitA, bitB, bitC, bitN, ns;
	dword bit0, bit1, bit2, bit3, bit4, bit5;
	dword ea, eb, ec, va, vb, vc;
#endif
	dword fa, fb, fc, fn, fe;
#ifndef USE_MU
	dword mult_const;
#else
	const dword mult_const = 1013;
#endif

	byte activereg;

#ifndef USE_FAST_AY
	dword vols[6][32];
#endif
	CHIP_TYPE chiptype;

	union {
		byte reg[16];
		struct AYREGS r;
	};

#ifndef USE_MU
	dword chip_clock_rate, system_clock_rate;
#else
	const dword chip_clock_rate = SNDR_DEFAULT_AY_RATE / 8;
	const dword system_clock_rate = SNDR_DEFAULT_SYSTICK_RATE;
#endif
	qword passed_chip_ticks, passed_clk_ticks;

	void _Reset(dword timestamp = 0); // call with default parameter, when context outside start_frame/end_frame block
	void Flush(dword chiptick, bool eof);
	void ApplyRegs(dword timestamp = 0);
};

#ifdef USE_FAST_AY
extern "C" {
// do not change order of this, as it is used by fast_ay.S
struct fast_ay_state {
	// note these are 5 bit values, with bit 4 being channel envelope flag
	uint8_t bits_a03x;
	uint8_t bits_b14x;
	uint8_t bits_c25x;
	uint8_t bit_n;

	uint8_t _pad[2];

	uint16_t fa, fb, fc, fn, fe;

	uint16_t a_output;
	uint16_t b_output;
	uint16_t c_output;
	uint16_t a_vol;
	uint16_t b_vol;
	uint16_t c_vol;
	uint16_t e_vol;
	uint16_t de_vol;

	uint32_t master_volume;
	uint32_t ns;
	uint32_t pad4[2];

	// mutable channel state
	// unlike original code, these tx count down not up, so must be adjust when fx changes
	uint16_t ta;
	uint16_t tb;
	uint16_t tc;
	uint16_t tn;
	uint16_t te;
	uint16_t tt;

	uint32_t v10001;                    // always 0x10001
	// sample state
	uint32_t frac_major;
	uint32_t frac_minor;
	// mutable sample state
	uint32_t frac_accum;
	uint32_t over_c_value;
	uint32_t over_rem;                  // number of over_samples remaining
	uint32_t over_accum;

	uint32_t wakeups;
};

static_assert(offsetof(struct fast_ay_state, fe) == 14, "");
static_assert(offsetof(struct fast_ay_state, ta) == 48, "");
static_assert(offsetof(struct fast_ay_state, frac_major) == 64, "");
static_assert(offsetof(struct fast_ay_state, wakeups) == 88, "");
extern uint32_t fast_ay(struct fast_ay_state *state, int16_t *samples, uint32_t max_samples);
}
#endif
#endif
#endif//__AY_H__
