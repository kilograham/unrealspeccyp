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

#ifndef NO_USE_AY
#include "../../std.h"
#include "../../z80/z80.h"
#include "ay.h"
#if KHAN128_I2S
#include "pico/audio_i2s.h"
#else
#include "pico/audio_pwm.h"
#endif
#include "hardware/gpio.h"
#include "../../options_common.h"

CU_REGISTER_DEBUG_PINS(generate)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(generate)

// set to 2 just to prove that 22050 is acceptable sounding
//#define HALVEIT 2
#define HALVEIT 1
#if PICO_ON_DEVICE
#define DOUBLEO_O 2
#else
// 2 is more oversampling
#define DOUBLEO_O 2
//#define DOUBLEO_O 1
#endif

#if HALVEIT == 2
#define DOUBLEO (DOUBLEO_O + 1)
#else
#define DOUBLEO DOUBLEO_O
#endif
#ifdef USE_FAST_AY
static struct fast_ay_state ays;
#endif

#define RATIO_MAJOR (SNDR_DEFAULT_AY_RATE / 8)
#define RATIO_MINOR ((SNDR_DEFAULT_SAMPLE_RATE << DOUBLEO) / HALVEIT)

//=============================================================================
//	eAY::eAY
//-----------------------------------------------------------------------------
eAY::eAY() : t(0)
#ifndef USE_FAST_AY
		,ta(0), tb(0), tc(0), tn(0), te(0)
	,env(0), denv(0)
	,bitA(0), bitB(0), bitC(0), bitN(0),ns(0)
	,bit0(0), bit1(0), bit2(0), bit3(0), bit4(0), bit5(0)
	,ea(0), eb(0), ec(0), va(0), vb(0), vc(0)
#endif
		,fa(0), fb(0), fc(0), fn(0), fe(0)
		,activereg(0)
{
#ifdef USE_FAST_AY
	ays.v10001 = 0x10001;
	ays.frac_major = RATIO_MAJOR;
	ays.frac_minor = RATIO_MINOR;
	static_assert(RATIO_MAJOR > RATIO_MINOR);
#endif

	SetChip(CHIP_AY);
	SetTimings(SNDR_DEFAULT_SYSTICK_RATE, SNDR_DEFAULT_AY_RATE, SNDR_DEFAULT_SAMPLE_RATE);
#ifndef USE_FAST_AY
	SetVolumes(0x7FFF, SNDR_VOL_AY, SNDR_PAN_ABC);
#endif

	_Reset();
}
#ifndef USE_HACKED_DEVICE_ABSTRACTION
//=============================================================================
//	eAY::IoRead
//-----------------------------------------------------------------------------
bool eAY::IoRead(word port) const
{
	return (port&0xC0FF) == 0xC0FD;
}
//=============================================================================
//	eAY::IoWrite
//-----------------------------------------------------------------------------
bool eAY::IoWrite(word port) const
{
	if(port&2)
		return false;
	if((port & 0xC0FF) == 0xC0FD)
		return true;
	if((port & 0xC000) == 0x8000)
		return true;
	return false;
}
//=============================================================================
//	eAY::IoRead
//-----------------------------------------------------------------------------
void eAY::IoRead(word port, byte* v, int tact)
{
	*v = Read();
}
//=============================================================================
//	eAY::IoWrite
//-----------------------------------------------------------------------------
void eAY::IoWrite(word port, byte v, int tact)
{
	if((port & 0xC0FF) == 0xC0FD)
	{
		Select(v);
	}
	if((port & 0xC000) == 0x8000)
	{
		Write(tact, v);
	}
}
#endif

const dword MULT_C_1 = 14; // fixed point precision for 'system tick -> ay tick'
// b = 1+ln2(max_ay_tick/8) = 1+ln2(max_ay_fq/8 / min_intfq) = 1+ln2(10000000/(10*8)) = 17.9
// assert(b+MULT_C_1 <= 32)

//=============================================================================
//	eAY::FrameStart
//-----------------------------------------------------------------------------
void eAY::FrameStart(dword tacts)
{
	t = tacts * chip_clock_rate / system_clock_rate;
	eInherited::FrameStart(t);
}
#ifdef USE_MUx
#define mult_const ((dword)(((qword)SNDR_DEFAULT_SYSTICK_RATE << (MULT_C_1 - 4)) / SNDR_DEFAULT_SYSTICK_RATE))
#endif

//=============================================================================
//	eAY::FrameEnd
//-----------------------------------------------------------------------------
void eAY::FrameEnd(dword tacts)
{
	//adjusting 't' with whole history will fix accumulation of rounding errors
//    qword end_chip_tick = ((passed_clk_ticks + tacts) * chip_clock_rate) / system_clock_rate;

	dword end_chip_tick = ((passed_clk_ticks + tacts) * mult_const) >> MULT_C_1;
	Flush(end_chip_tick - passed_chip_ticks, true);
	eInherited::FrameEnd(t);
	passed_clk_ticks += tacts;
	passed_chip_ticks += t;
}

#define DOUBLEO_MASK ((1u<<DOUBLEO)-1u)

struct audio_buffer *buffer = 0;
//=============================================================================
//	eAY::Flush
//-----------------------------------------------------------------------------
void __attribute__((noinline)) eAY::Flush(dword chiptick, bool eof)
{
	if (!buffer) {
		buffer = take_audio_buffer(producer_pool, true);
		buffer->sample_count = 0;
	}
	int16_t *samples = (int16_t *) buffer->buffer->bytes;
#ifdef USE_FAST_AY
	int count = chiptick - t;
	if (count < 0 || count > 4434) {
		static bool pants;
		if (!pants) {
			printf("warn out of range %d\n", count);
			pants = true;
		}
		if (count < 0) count = 0;
		else count = 4434;
	}


	uint v = xPlatform::OpSoundVolume();
//    printf("vol %d\n", v);
	ays.master_volume = v * 13; // now 4 fractional bits in fast_ay.S) MIN(v, 8);// ? (1  + ((v*11)>>4)) : 0;
	//ays.wakeups = 0;
	bool print = false;
	assert(count < 65531);
	if (count > 0)
	{
		ays.tt = count;
		if (print) printf("-> %d %d %d %d %d %d\n", count,  ays.tt, (uint)buffer->sample_count, (uint)buffer->max_sample_count, (uint)(ays.over_c_value >> 30u), (uint)ays.over_rem);
		DEBUG_PINS_SET(generate, 2);
		//    printf("count %d\n", count);
//        assert(!ays.over_rem); // should not be any remaining samples
//        assert(!buffer->sample_count);
#if 0
		ays.a_vol = 28;
		ays.fa = 50;
		if (!ays.ta) ays.ta = ays.fa + 1;
		ays.bits_a03x |= 2;
		ays.bits_a03x &= ~4;
		static bool warble;
//        if (warble) __breakpoint();
#endif
#if 1
		uint32_t written = fast_ay(&ays, samples + buffer->sample_count,
								   buffer->max_sample_count - buffer->sample_count);
#else
		uint32_t written = 0;
		printf("and...\n");
		int e = ays.tt;
		for(int j=0; j<e; j++) {
			ays.tt = 1;
			written += fast_ay(&ays, samples + buffer->sample_count + written,
									   buffer->max_sample_count - buffer->sample_count - written);
			printf("%d %d\n", j, written);
		}
#endif
        static_assert(PICO_AUDIO_BUFFER_SAMPLE_LENGTH >= (PICO_SAMPLE_RATE + 49) / 50, "");
        if (ays.over_rem) {
            printf("OOPS overflow %d\n", ays.over_rem);
        }
		assert(!ays.over_rem); // should not be any remaining samples
		DEBUG_PINS_CLR(generate, 2);
		buffer->sample_count += written;
		assert(buffer->sample_count <= buffer->max_sample_count);
		if (print) printf("      %d %d %d %d %d\n", (int)written, ays.tt, (uint)buffer->sample_count, (uint)ays.over_rem, ays.te);
#if 0
		printf("--\n");
		for(int i=0;i<buffer->sample_count;i += 16) {
			for(int j=i; j<MIN(buffer->sample_count, i+ 16); j++) {
				//printf("%04x ", samples[j]);
				printf("%d\n", samples[j]);
			}
			//printf("\n");
		}
#endif
//        if (samples[0]) warble = true;
	} else {
		assert(count == 0);
		assert(eof); // only time we should have no ticks
	}
	t = chiptick;
	if (buffer->sample_count && eof)
	{
		if (print) printf("%d\n", (uint)buffer->sample_count);
//        int x = buffer->sample_count;
//        int y = buffer->max_sample_count;
		DEBUG_PINS_SET(generate, 1);
		give_audio_buffer(producer_pool, buffer);
		DEBUG_PINS_CLR(generate, 1);
//        printf("%d %d\n", x, y);
		buffer = 0;
	}

#else
	//const int32_t RATIO_MAJOR = chip_clock_rate;
	int32_t ratio = RATIO_MAJOR / 2;
	static bool flip;
	if (!flip) {
		printf("%f %d %d\n", ((float)(RATIO_MAJOR)) / RATIO_MINOR, RATIO_MAJOR, SNDR_DEFAULT_SAMPLE_RATE * 2);
		flip = 1;
	}
	uint alt = DOUBLEO_MASK;
	dword en, mix_l = 0, mix_r = 0;
//    dword x = 100000;
//    if (fa) x = MIN(x, fa);
//    if (fb) x = MIN(x, fb);
//    if (fc) x = MIN(x, fc);
//    if (fn) x = MIN(x, fn);
//    if (fe) x = MIN(x, fe);
//    printf("%d %d %d %d\nn", x, wakeups, wobble, chiptick - t);
	DEBUG_PINS_SET(generate, 1);
	while (t < chiptick)
	{
		t++;
		if (++ta >= fa) ta = 0, bitA ^= -1;
		if (++tb >= fb) tb = 0, bitB ^= -1;
		if (++tc >= fc) tc = 0, bitC ^= -1;
		if (++tn >= fn)
			tn = 0,
			ns = (ns * 2 + 1) ^ (((ns >> 16) ^ (ns >> 13)) & 1),
			bitN = 0 - ((ns >> 16) & 1);
		if (++te >= fe)
		{
			te = 0, env += denv;
			if (env & ~31)
			{
				dword mask = (1 << r.env);
				if (mask &
					((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 9) |
					 (1 << 15)))
					env = denv = 0;
				else if (mask & ((1 << 8) | (1 << 12)))
					env &= 31;
				else if (mask & ((1 << 10) | (1 << 14)))
					denv = -denv, env = env + denv;
				else env = 31, denv = 0; //11,13
			}
		}


#if DOUBLEO
		if (++alt > DOUBLEO_MASK)
		{
			mix_l = mix_r = 0;
			alt = 0;
		}
#else
		mix_l = mix_r = 0;
#endif
		en = ((ea & env) | va) & ((bitA | bit0) & (bitN | bit3));
		mix_l += vols[0][en];
		mix_r += vols[1][en];

		en = ((eb & env) | vb) & ((bitB | bit1) & (bitN | bit4));
		mix_l += vols[2][en];
		mix_r += vols[3][en];

		en = ((ec & env) | vc) & ((bitC | bit2) & (bitN | bit5));
		mix_l += vols[4][en];
		mix_r += vols[5][en];


		static_assert(RATIO_MAJOR >= RATIO_MINOR, "");
//        if((mix_l ^ eInherited::mix_l) | (mix_r ^ eInherited::mix_r)) // similar check inside update()
//            Update(t, mix_l, mix_r);
#if DOUBLEO
		if (alt == DOUBLEO_MASK)
		{
			ratio -= RATIO_MINOR;
#else
			ratio -= SNDR_DEFAULT_SAMPLE_RATE;
#endif
		if (ratio <= 0)
		{
			samples[buffer->sample_count++] = (mix_l + mix_r) / (2 << DOUBLEO);
#if HALVEIT == 2
				samples[buffer->sample_count++] = (mix_l + mix_r) / (2 << DOUBLEO);
#endif
				assert(buffer->sample_count <= buffer->max_sample_count);
			if (buffer->sample_count == buffer->max_sample_count)
			{
				give_audio_buffer(producer_pool, buffer);
				buffer = take_audio_buffer(producer_pool, true);
				samples = (int16_t *) buffer->buffer->bytes;
				buffer->sample_count = 0;
			}
			ratio += RATIO_MAJOR;
		}
#if DOUBLEO
	}
#endif
	}
	DEBUG_PINS_CLR(generate, 1);
#endif
}
//=============================================================================
//	eAY::Select
//-----------------------------------------------------------------------------
void eAY::Select(byte nreg)
{
	if(chiptype == CHIP_AY) nreg &= 0x0F;
	activereg = nreg;
#ifdef USE_FAST_AY
	extern uint16_t *current_voltab;
	extern uint16_t voltab_ay;
	extern uint16_t voltab_ym;
	current_voltab = chiptype == CHIP_AY ? &voltab_ay : &voltab_ym;
#endif
}

#ifdef USE_FAST_AY
static inline uint16_t bswap_and_adjust(uint16_t v) {
	v = SwapWord(v);
	if (v < 3) v = 3; // seems like a reasonable minimum freq
	return v;
}

void adjust_tx(uint16_t new_fx, uint16_t &fx, uint16_t &tx) {
	new_fx &= 0xfff;
	if (!fx) tx = 1; // we should have been ready anyway
	fx = new_fx;
	if (!fx) tx = 0xffff; // hack to avoid waking up all the time
}

void update_channel_bits(uint8_t &cb, uint8_t x0, uint8_t x3) {
	cb = (cb & 0x9) | (x0 << 2) | (x3 << 1);
}
#endif
//=============================================================================
//	eAY::Write
//-----------------------------------------------------------------------------
void eAY::Write(dword timestamp, byte val)
{
	if(activereg >= 0x10)
		return;

	if((1 << activereg) & ((1<<1)|(1<<3)|(1<<5)|(1<<13))) val &= 0x0F;
	if((1 << activereg) & ((1<<6)|(1<<8)|(1<<9)|(1<<10))) val &= 0x1F;

	if(activereg != 13 && reg[activereg] == val)
		return;

	reg[activereg] = val;

	if(timestamp)
		Flush((timestamp * mult_const) >> MULT_C_1, false); // cputick * ( (chip_clock_rate/8) / system_clock_rate );

#ifdef USE_FAST_AY
	switch(activereg)
	{
		case 0:
		case 1:
			adjust_tx(bswap_and_adjust(r.fA), ays.fa, ays.ta);
			break;
		case 2:
		case 3:
			adjust_tx(bswap_and_adjust(r.fB), ays.fb, ays.tb);
			break;
		case 4:
		case 5:
			adjust_tx(bswap_and_adjust(r.fC), ays.fc, ays.tc);
			break;
		case 6:
		{
			// todo noise freq needs more work (with oversampling)
//            uint fn = ((val&0x1f) * 9)/4;
			//if (fn == 1) fn = 2;
			//uint fn = (val&0x1f)*2 + 1;
			uint fn = (val&0x1f)*2;
			adjust_tx(fn, ays.fn, ays.tn);
			break;
		}
		case 7:
			update_channel_bits(ays.bits_a03x, (val >> 0u) & 1u, (val >> 3u) & 1u);
			update_channel_bits(ays.bits_b14x, (val >> 1u) & 1u, (val >> 4u) & 1u);
			update_channel_bits(ays.bits_c25x, (val >> 2u) & 1u, (val >> 5u) & 1u);
			break;
		case 8:
			ays.bits_a03x &= ~0x10;
			ays.bits_a03x |= (val&0x10);
			ays.a_vol = ((val & 0x0F)*2+1);
			break;
		case 9:
			ays.bits_b14x &= ~0x10;
			ays.bits_b14x |= (val&0x10);
			ays.b_vol = ((val & 0x0F)*2+1);
			break;
		case 10:
			ays.bits_c25x &= ~0x10;
			ays.bits_c25x |= (val&0x10);
			ays.c_vol = ((val & 0x0F)*2+1);
			break;
		case 11:
		case 12:
			adjust_tx( bswap_and_adjust(r.envT), ays.fe, ays.te);
			break;
		case 13:
		{
			ays.te = ays.fe; // reset timer
			if (ays.te <= 1) ays.te = 0xffff;
			if (r.env & 4) ays.e_vol = 0, ays.de_vol = 1; // attack
			else ays.e_vol = 31, ays.de_vol = -1; // decay
			extern uint32_t envelope_switch[];
			extern uint32_t envelope_handler;
			envelope_handler = envelope_switch[r.env & 0xf];
			break;
		}
	}
#else
	switch(activereg)
	{
	case 0:
	case 1:
		fa = SwapWord(r.fA);
		break;
	case 2:
	case 3:
		fb = SwapWord(r.fB);
		break;
	case 4:
	case 5:
		fc = SwapWord(r.fC);
		break;
	case 6:
		fn = val*2;
		break;
	case 7:
		bit0 = 0 - ((val>>0) & 1);
		bit1 = 0 - ((val>>1) & 1);
		bit2 = 0 - ((val>>2) & 1);
		bit3 = 0 - ((val>>3) & 1);
		bit4 = 0 - ((val>>4) & 1);
		bit5 = 0 - ((val>>5) & 1);
		break;
	case 8:
		ea = (val & 0x10)? -1 : 0;
		va = ((val & 0x0F)*2+1) & ~ea;
		break;
	case 9:
		eb = (val & 0x10)? -1 : 0;
		vb = ((val & 0x0F)*2+1) & ~eb;
		break;
	case 10:
		ec = (val & 0x10)? -1 : 0;
		vc = ((val & 0x0F)*2+1) & ~ec;
		break;
	case 11:
	case 12:
		fe = SwapWord(r.envT);
		break;
	case 13:
		te = 0;
		if(r.env & 4) env = 0, denv = 1; // attack
		else env = 31, denv = -1; // decay
		break;
	}
#endif
}
//=============================================================================
//	eAY::Read
//-----------------------------------------------------------------------------
byte eAY::Read()
{
	if(activereg >= 0x10)
		return 0xFF;
	return reg[activereg & 0x0F];
}
//=============================================================================
//	eAY::SetTimings
//-----------------------------------------------------------------------------
void eAY::SetTimings(dword _system_clock_rate, dword _chip_clock_rate, dword _sample_rate)
{

#ifndef USE_MU
	_chip_clock_rate /= 8;
	system_clock_rate = _system_clock_rate;
	chip_clock_rate = _chip_clock_rate;
	mult_const = (dword)(((qword)chip_clock_rate << MULT_C_1) / system_clock_rate);
#endif

#ifndef USE_MU
	eInherited::SetTimings(_chip_clock_rate, _sample_rate);
#endif
	passed_chip_ticks = passed_clk_ticks = 0;
	t = 0;
#ifndef USE_FAST_AY
	ns = 0xFFFF;
#else
	ays.ns = 0xffff;
#endif
}
#ifndef USE_FAST_AY
//=============================================================================
//	eAY::SetVolumes
//-----------------------------------------------------------------------------
void eAY::SetVolumes(dword global_vol, const SNDCHIP_VOLTAB *voltab, const SNDCHIP_PANTAB *stereo)
{
	for (int j = 0; j < 6; j++)
		for (int i = 0; i < 32; i++)
			vols[j][i] = (dword)(((qword)global_vol * voltab->v[i] * stereo->raw[j])/(65535*100*3));
}
#endif
//#pragma GCC push_options
//#pragma GCC optimize("Os")
//=============================================================================
//	eAY::Reset
//-----------------------------------------------------------------------------
void eAY::_Reset(dword timestamp)
{
	for(int i = 0; i < 16; i++)
		reg[i] = 0;
	ApplyRegs(timestamp);
}
//#pragma GCC pop_options
//=============================================================================
//	eAY::ApplyRegs
//-----------------------------------------------------------------------------
void eAY::ApplyRegs(dword timestamp)
{
	byte ar = activereg;
	for(byte r = 0; r < 16; r++)
	{
		Select(r);
		byte p = reg[r];
		/* clr cached values */
		Write(timestamp, p ^ 1);
		Write(timestamp, p);
	}
	activereg = ar;
}

// corresponds enum CHIP_TYPE
const char * const ay_chips[] = { "AY-3-8910", "YM2149F" };

const char* eAY::GetChipName(CHIP_TYPE i) { return ay_chips[i]; }

#ifndef USE_FAST_AY
const SNDCHIP_VOLTAB SNDR_VOL_AY_S =
{ { 0x0000,0x0000,0x0340,0x0340,0x04C0,0x04C0,0x06F2,0x06F2,0x0A44,0x0A44,0x0F13,0x0F13,0x1510,0x1510,0x227E,0x227E,
	0x289F,0x289F,0x414E,0x414E,0x5B21,0x5B21,0x7258,0x7258,0x905E,0x905E,0xB550,0xB550,0xD7A0,0xD7A0,0xFFFF,0xFFFF } };

const SNDCHIP_VOLTAB SNDR_VOL_YM_S =
{ { 0x0000,0x0000,0x00EF,0x01D0,0x0290,0x032A,0x03EE,0x04D2,0x0611,0x0782,0x0912,0x0A36,0x0C31,0x0EB6,0x1130,0x13A0,
	0x1751,0x1BF5,0x20E2,0x2594,0x2CA1,0x357F,0x3E45,0x475E,0x5502,0x6620,0x7730,0x8844,0xA1D2,0xC102,0xE0A2,0xFFFF } };

static const SNDCHIP_PANTAB SNDR_PAN_MONO_S = {{ 58, 58,  58,58,   58,58 }};
static const SNDCHIP_PANTAB SNDR_PAN_ABC_S =  {{ 100,10,  66,66,   10,100}};
static const SNDCHIP_PANTAB SNDR_PAN_ACB_S =  {{ 100,10,  10,100,  66,66 }};
static const SNDCHIP_PANTAB SNDR_PAN_BAC_S =  {{ 66,66,   100,10,  10,100}};
static const SNDCHIP_PANTAB SNDR_PAN_BCA_S =  {{ 10,100,  100,10,  66,66 }};
static const SNDCHIP_PANTAB SNDR_PAN_CAB_S =  {{ 66,66,   10,100,  100,10}};
static const SNDCHIP_PANTAB SNDR_PAN_CBA_S =  {{ 10,100,  66,66,   100,10}};

const SNDCHIP_VOLTAB* SNDR_VOL_AY = &SNDR_VOL_AY_S;
const SNDCHIP_VOLTAB* SNDR_VOL_YM = &SNDR_VOL_YM_S;
const SNDCHIP_PANTAB* SNDR_PAN_MONO = &SNDR_PAN_MONO_S;
const SNDCHIP_PANTAB* SNDR_PAN_ABC = &SNDR_PAN_ABC_S;
const SNDCHIP_PANTAB* SNDR_PAN_ACB = &SNDR_PAN_ACB_S;
const SNDCHIP_PANTAB* SNDR_PAN_BAC = &SNDR_PAN_BAC_S;
const SNDCHIP_PANTAB* SNDR_PAN_BCA = &SNDR_PAN_BCA_S;
const SNDCHIP_PANTAB* SNDR_PAN_CAB = &SNDR_PAN_CAB_S;
const SNDCHIP_PANTAB* SNDR_PAN_CBA = &SNDR_PAN_CBA_S;
#endif
#endif