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

#include "../std.h"

#include "ula.h"
#include "memory.h"

#define Max(o, p)	(o > p ? o : p)
#define Min(o, p)	(o < p ? o : p)

#define line_tacts 224
#define paper_start	17989
#define mid_lines SZX_HEIGHT
#define buf_mid SZX_WIDTH
#define b_top ((S_HEIGHT - mid_lines) / 2)
#define b_left ((S_WIDTH - buf_mid) / 2)
#define first_line_t (paper_start - b_top * line_tacts - b_left / 2)

// this produces very nice code on ARM!
#define hideous_divide_by_224(x) (((((x)*0x1249) + (((x)*0x1249)>>15)) + 255)>>20)

#define speccy_y_to_normal(y) (((y) & 0xc0) + (((y)&7) << 3) + (((y) >> 3) & 7))
// it is self inverse
#define normal_to_speccy_y(y) speccy_y_to_normal(y)
//=============================================================================
//	eUla::eUla
//-----------------------------------------------------------------------------
eUla::eUla(eMemory* m) : memory(m), border_color(0), first_screen(true), base(NULL),
						 prev_t(0), border_y(0), in_paper(false), frame(0)
#ifndef NO_USE_128K
		,mode_48k(false)
#endif
{
}
//=============================================================================
//	eUla::~eUla
//-----------------------------------------------------------------------------
#ifndef NO_USE_DESTRUCTORS
eUla::~eUla()
{
#ifndef NO_USE_SCREEN
	SAFE_DELETE_ARRAY(screen);
#endif
}
#endif
//=============================================================================
//	eUla::Init
//-----------------------------------------------------------------------------
void eUla::Init()
{
	border_y = prev_t = 0;
	in_paper = false;
#ifndef NO_USE_SCREEN
	colortab = colortab1;
#endif
	base = memory->Get(eMemory::P_RAM5);
}
//=============================================================================
//	eUla::Reset
//-----------------------------------------------------------------------------
void eUla::Reset()
{
#ifndef NO_USE_128K
	SwitchScreen(true, 0);
#endif
}
#ifndef NO_USE_128K
//=============================================================================
//	eUla::SwitchScreen
//-----------------------------------------------------------------------------
void eUla::SwitchScreen(bool first, int tact)
{
	if(first == first_screen)
		return;
	UpdateRay(tact);
	first_screen = first;
	int page = first_screen ? eMemory::P_RAM5: eMemory::P_RAM7;
	base = memory->Get(page);
}
#endif
#ifndef USE_HACKED_DEVICE_ABSTRACTION
//=============================================================================
//	eUla::IoRead
//-----------------------------------------------------------------------------
bool eUla::IoRead(word port) const
{
	return (port&0xff) == 0xff;
}
//=============================================================================
//	eUla::IoWrite
//-----------------------------------------------------------------------------
bool eUla::IoWrite(word port) const
{
	return !(port&1) || (!mode_48k && !(port & 2) && !(port & 0x8000));
}
#endif
//=============================================================================
//	eUla::IoRead
//-----------------------------------------------------------------------------
void eUla::IoRead(word port, byte* v, int tact)
{
	UpdateRay(tact);
	if(!in_paper) // ray is not in paper
	{
		*v = 0xff;
		return;
	}
	byte* atr = base + 0x1800 + (paper_y / 8) * 32 + paper_x / 4;
	*v = *atr;
}
//=============================================================================
//	eUla::IoWrite
//-----------------------------------------------------------------------------
void eUla::IoWrite(word port, byte v, int tact)
{
	if(!(port & 1)) // port 0xfe
	{
		if((v & 7) != border_color)
		{
			UpdateRay(tact);
			border_color = v & 7;
		}
	}
#ifndef NO_USE_128K
	if(!(port & 2) && !(port & 0x8000)) // zx128 port
	{
		SwitchScreen(!(v & 0x08), tact);
	}
#endif
}
//=============================================================================
//	eUla::FrameUpdate
//-----------------------------------------------------------------------------
void eUla::FrameUpdate()
{
	UpdateRay(0x7fff0000);
	in_paper = false;
	border_y = 0;
	if(++frame >= 15)
	{
		frame = 0;
	}
}



//=============================================================================
//	UpdateRay
//-----------------------------------------------------------------------------
void eUla::UpdateRay(int tact)
{
	int t = tact - first_line_t;
	if (t > 0) {
		// y = t / 224;
		int32_t y = hideous_divide_by_224(t);
		for(; border_y < Min(y, S_HEIGHT); border_y++) {
			// todo left/right border or indeed changing mid line
			border_colors[border_y] = border_color;
		}
		in_paper = y >= b_top && y < (b_top + mid_lines);
		if (in_paper) {
			paper_x = t - y * 224 - b_left / 2;
			paper_x = paper_x * 2;
			if (paper_x < 0 || paper_x >= buf_mid) {
				in_paper = false;
			} else {
				y -= b_top;
				paper_y = speccy_y_to_normal(y);
			}
		}
	}
	prev_t = tact;
}

// note l is 0 based for top of display area, negative for top border
bool eUla::GetLineInfo(int l, byte& border, const byte *& attr, const byte *& pixels) {
	int sl = l + (S_HEIGHT - SZX_HEIGHT) / 2;
	if (sl < 0) sl = 0;
	if (sl >= S_HEIGHT) sl = S_HEIGHT - 1;
	border = border_colors[sl];
	if (l >= 0 && l < SZX_HEIGHT) {
		pixels = base + normal_to_speccy_y(l) * 32;
		attr = base + 0x1800 + (l >> 3) * 32;
	} else {
		pixels = attr = nullptr;
	}
	return true;
}