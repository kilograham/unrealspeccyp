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

#ifndef	__ULA_H__
#define	__ULA_H__

#include "device.h"

#pragma once

class eMemory;

//*****************************************************************************
//	eUla
//-----------------------------------------------------------------------------
class eUla : public eDevice
{
public:
	eUla(eMemory* m);
#ifndef NO_USE_DESTRUCTORS
	virtual ~eUla();
#endif
	virtual void Init();
	virtual void Reset();
	virtual void FrameUpdate();

#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoRead(word port) const final;
	virtual bool IoWrite(word port) const final;
	virtual dword IoNeed() const { return ION_WRITE|ION_READ; }
#endif
	virtual void IoRead(word port, byte* v, int tact) final;
	virtual void IoWrite(word port, byte v, int tact) final;
	void	Write(int tact) {
		if(prev_t < tact) UpdateRay(tact);
	}

	byte	BorderColor() const { return border_color; }
	bool	FirstScreen() const { return first_screen; }
#ifndef NO_USE_128K
	void	Mode48k(bool on)	{ mode_48k = on; }
#endif

	static eDeviceId Id() { return D_ULA; }
	bool    GetLineInfo(int l, byte& border, const byte *& attr, const byte *& pixels);
#ifndef NO_USE_128K
	void	SwitchScreen(bool first, int tact);
#endif
protected:
	void	UpdateRay(int tact);

	enum eScreen { S_WIDTH = 320, S_HEIGHT = 240, SZX_WIDTH = 256, SZX_HEIGHT = 192 };
protected:
	eMemory* memory;
	int		line_tacts;		// t-states per line
	int		paper_start;	// start of paper
	byte	border_color;
	bool	first_screen;
	byte*	base;

	int     prev_t;
	int		border_y;		// last update ray y pos
	bool    in_paper;
	int		frame;
	// only valid if in_paper = true;
	int     paper_x, paper_y;
#ifndef NO_USE_128K
	bool	mode_48k;
#else
	static const bool mode_48k = false;
#endif
	byte    border_colors[S_HEIGHT];
};

#endif//__ULA_H__
