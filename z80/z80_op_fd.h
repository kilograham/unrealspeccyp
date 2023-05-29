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

/* FD prefix opcodes */

#ifndef	__Z80_OP_FD_H__
#define	__Z80_OP_FD_H__

#pragma once

#undef OPXY_FUNC
#define OPXY_FUNC(a) Opy##a
#undef xy_reg
#define xy_reg iy
#undef xy_reg_lo
#define xy_reg_lo yl
#undef xy_reg_hi
#define xy_reg_hi yh

#include "z80_op_xy.h"

#endif//__Z80_OP_FD_H__
