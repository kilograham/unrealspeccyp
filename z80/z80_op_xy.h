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

/* DD prefix opcodes */

void OPXY_FUNC(09)() { // add ix,bc
	add16(xy_reg, bc);
}

void OPXY_FUNC(19)() { // add ix,de
	add16(xy_reg, de);
}
void OPXY_FUNC(21)() { // ld ix,nnnn
	xy_reg = Read2Inc(pc);
	t += 6;
}
void OPXY_FUNC(22)() { // ld (nnnn),ix
	temp16_xy adr = Read2Inc(pc);
	memptr = adr+1;
	Write2(adr, xy_reg);
	t += 12;
}
void OPXY_FUNC(23)() { // inc ix
	xy_reg++;
	t += 2;
}
void OPXY_FUNC(24)() { // inc xh
	inc8(xy_reg_hi);
}
void OPXY_FUNC(25)() { // dec xh
	dec8(xy_reg_hi);
}
void OPXY_FUNC(26)() { // ld xh,nn
	xy_reg_hi = ReadInc(pc);
	t += 3;
}
void OPXY_FUNC(29)() { // add ix,ix
//    memptr = xy_reg+1;
//	f = (f & ~(NF | CF | F5 | F3 | HF));
//	f |= ((xy_reg >> 7) & 0x10); /* HF */
//	xy_reg = (xy_reg & 0xFFFF)*2;
//	if (xy_reg & 0x10000) f |= CF;
//	f |= (xy_reg_hi & (F5 | F3));
//	t += 7;
	add16(xy_reg, xy_reg);
}
void OPXY_FUNC(2A)() { // ld ix,(nnnn)
	temp16_xy adr = Read2Inc(pc);
	xy_reg = Read2(adr);
	memptr = adr+1;
	t += 12;
}
void OPXY_FUNC(2B)() { // dec ix
	--xy_reg;
	t += 2;
}
void OPXY_FUNC(2C)() { // inc xl
	inc8(xy_reg_lo);
}
void OPXY_FUNC(2D)() { // dec xl
	dec8(xy_reg_lo);
}
void OPXY_FUNC(2E)() { // ld xl,nn
	xy_reg_lo = ReadInc(pc);
	t += 3;
}
void OPXY_FUNC(34)() { // inc (ix+nn)
	scratch16 scratch = xy_reg + (address_delta)ReadInc(pc);
	temp8_xy v = Read(scratch);
	inc8(v);
	WriteXY(scratch, v);
	t += 15;
}
void OPXY_FUNC(35)() { // dec (ix+nn)
	scratch16 scratch = xy_reg + (address_delta)ReadInc(pc);
	temp8_xy v = Read(scratch);
	dec8(v);
	WriteXY(scratch, v);
	t += 15;
}
void OPXY_FUNC(36)() { // ld (ix+nn),nn
	scratch16 scratch = xy_reg + (address_delta)ReadInc(pc);
	temp8_xy v = ReadInc(pc);
	WriteXY(scratch, v);
	t += 11;
}
void OPXY_FUNC(39)() { // add ix,sp
	add16(xy_reg, sp);
}

void OPXY_FUNC(44)() { // ld b,xh
	b = xy_reg_hi;
}
void OPXY_FUNC(45)() { // ld b,xl
	b = xy_reg_lo;
}
void OPXY_FUNC(46)() { // ld b,(ix+nn)
	b = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(4C)() { // ld c,xh
	c = xy_reg_hi;
}
void OPXY_FUNC(4D)() { // ld c,xl
	c = xy_reg_lo;
}
void OPXY_FUNC(4E)() { // ld c,(ix+nn)
	c = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(54)() { // ld d,xh
	d = xy_reg_hi;
}
void OPXY_FUNC(55)() { // ld d,xl
	d = xy_reg_lo;
}
void OPXY_FUNC(56)() { // ld d,(ix+nn)
	d = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(5C)() { // ld e,xh
	e = xy_reg_hi;
}
void OPXY_FUNC(5D)() { // ld e,xl
	e = xy_reg_lo;
}
void OPXY_FUNC(5E)() { // ld e,(ix+nn)
	e = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(60)() { // ld xh,b
	xy_reg_hi = b;
}
void OPXY_FUNC(61)() { // ld xh,c
	xy_reg_hi = c;
}
void OPXY_FUNC(62)() { // ld xh,d
	xy_reg_hi = d;
}
void OPXY_FUNC(63)() { // ld xh,e
	xy_reg_hi = e;
}
void OPXY_FUNC(65)() { // ld xh,xl
	xy_reg_hi = xy_reg_lo;
}
void OPXY_FUNC(66)() { // ld h,(ix+nn)
	h = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(67)() { // ld xh,a
	xy_reg_hi = a;
}
void OPXY_FUNC(68)() { // ld xl,b
	xy_reg_lo = b;
}
void OPXY_FUNC(69)() { // ld xl,c
	xy_reg_lo = c;
}
void OPXY_FUNC(6A)() { // ld xl,d
	xy_reg_lo = d;
}
void OPXY_FUNC(6B)() { // ld xl,e
	xy_reg_lo = e;
}
void OPXY_FUNC(6C)() { // ld xl,xh
	xy_reg_lo = xy_reg_hi;
}
void OPXY_FUNC(6E)() { // ld l,(ix+nn)
	l = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(6F)() { // ld xl,a
	xy_reg_lo = a;
}
void OPXY_FUNC(70)() { // ld (ix+nn),b
	WriteXY(xy_reg + ReadInc(pc), b);
	t += 11;
}
void OPXY_FUNC(71)() { // ld (ix+nn),c
	WriteXY(xy_reg + (address_delta)ReadInc(pc), c);
	t += 11;
}
void OPXY_FUNC(72)() { // ld (ix+nn),d
	WriteXY(xy_reg + (address_delta)ReadInc(pc), d);
	t += 11;
}
void OPXY_FUNC(73)() { // ld (ix+nn),e
	WriteXY(xy_reg + (address_delta)ReadInc(pc), e);
	t += 11;
}
void OPXY_FUNC(74)() { // ld (ix+nn),h
	WriteXY(xy_reg + (address_delta)ReadInc(pc), h);
	t += 11;
}
void OPXY_FUNC(75)() { // ld (ix+nn),l
	WriteXY(xy_reg + (address_delta)ReadInc(pc), l);
	t += 11;
}
void OPXY_FUNC(77)() { // ld (ix+nn),a
	WriteXY(xy_reg + (address_delta)ReadInc(pc), a);
	t += 11;
}
void OPXY_FUNC(7C)() { // ld a,xh
	a = xy_reg_hi;
}
void OPXY_FUNC(7D)() { // ld a,xl
	a = xy_reg_lo;
}
void OPXY_FUNC(7E)() { // ld a,(ix+nn)
	a = Read(xy_reg + (address_delta)ReadInc(pc));
	t += 11;
}
void OPXY_FUNC(84)() { // add a,xh
	add8(xy_reg_hi);
}
void OPXY_FUNC(85)() { // add a,xl
	add8(xy_reg_lo);
}
void OPXY_FUNC(86)() { // add a,(ix+nn)
	add8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(8C)() { // adc a,xh
	adc8(xy_reg_hi);
}
void OPXY_FUNC(8D)() { // adc a,xl
	adc8(xy_reg_lo);
}
void OPXY_FUNC(8E)() { // adc a,(ix+nn)
	adc8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(94)() { // sub xh
	sub8(xy_reg_hi);
}
void OPXY_FUNC(95)() { // sub xl
	sub8(xy_reg_lo);
}
void OPXY_FUNC(96)() { // sub (ix+nn)
	sub8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(9C)() { // sbc a,xh
	sbc8(xy_reg_hi);
}
void OPXY_FUNC(9D)() { // sbc a,xl
	sbc8(xy_reg_lo);
}
void OPXY_FUNC(9E)() { // sbc a,(ix+nn)
	sbc8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(A4)() { // and xh
	and8(xy_reg_hi);
}
void OPXY_FUNC(A5)() { // and xl
	and8(xy_reg_lo);
}
void OPXY_FUNC(A6)() { // and (ix+nn)
	and8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(AC)() { // xor xh
	xor8(xy_reg_hi);
}
void OPXY_FUNC(AD)() { // xor xy_reg_lo
	xor8(xy_reg_lo);
}
void OPXY_FUNC(AE)() { // xor (ix+nn)
	xor8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(B4)() { // or xh
	or8(xy_reg_hi);
}
void OPXY_FUNC(B5)() { // or xl
	or8(xy_reg_lo);
}
void OPXY_FUNC(B6)() { // or (ix+nn)
	or8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(BC)() { // cp xh
	cp8(xy_reg_hi);
}
void OPXY_FUNC(BD)() { // cp xl
	cp8(xy_reg_lo);
}
void OPXY_FUNC(BE)() { // cp (ix+nn)
	cp8(Read(xy_reg + (address_delta)ReadInc(pc)));
	t += 11;
}
void OPXY_FUNC(E1)() { // pop ix
	xy_reg = Read2Inc(sp);
	t += 6;
}
void OPXY_FUNC(E3)() { // ex (sp),ix
	memptr = Read2(sp);
	Write2(sp, xy_reg);
	xy_reg = memptr;
	t += 15;
}
void OPXY_FUNC(E5)() { // push ix
	push(xy_reg);
	t += 7;
}
void OPXY_FUNC(E9)() { // jp (ix)
	pc = xy_reg;
}
void OPXY_FUNC(F9)() { // ld sp,ix
	sp = xy_reg;
	t += 2;
}

