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

/* DDCB/FDCB opcodes */
/* note: cpu.t and destination updated in step(), here process 'v' */

#ifndef	__Z80_OP_DDCB_H__
#define	__Z80_OP_DDCB_H__

#pragma once

temp8 Oplx00(temp8 v) { // rlc (ix+nn)
	rlc8(v);
	return v;
}
temp8 Oplx08(temp8 v) { // rrc (ix+nn)
	rrc8(v);
	return v;
}
temp8 Oplx10(temp8 v) { // rl (ix+nn)
	rl8(v);
	return v;
}
temp8 Oplx18(temp8 v) { // rr (ix+nn)
	rr8(v);
	return v;
}
temp8 Oplx20(temp8 v) { // sla (ix+nn)
	sla8(v);
	return v;
}
temp8 Oplx28(temp8 v) { // sra (ix+nn)
	sra8(v);
	return v;
}
temp8 Oplx30(temp8 v) { // sli (ix+nn)
	sli8(v);
	return v;
}
temp8 Oplx38(temp8 v) { // srl (ix+nn)
	srl8(v);
	return v;
}
temp8 Oplx40(temp8 v) { // bit 0,(ix+nn)
	bitmem(v, 0); return v;
}
temp8 Oplx48(temp8 v) { // bit 1,(ix+nn)
	bitmem(v, 1); return v;
}
temp8 Oplx50(temp8 v) { // bit 2,(ix+nn)
	bitmem(v, 2); return v;
}
temp8 Oplx58(temp8 v) { // bit 3,(ix+nn)
	bitmem(v, 3); return v;
}
temp8 Oplx60(temp8 v) { // bit 4,(ix+nn)
	bitmem(v, 4); return v;
}
temp8 Oplx68(temp8 v) { // bit 5,(ix+nn)
	bitmem(v, 5); return v;
}
temp8 Oplx70(temp8 v) { // bit 6,(ix+nn)
	bitmem(v, 6); return v;
}
temp8 Oplx78(temp8 v) { // bit 7,(ix+nn)
	bitmem(v, 7); return v;
}
temp8 Oplx80(temp8 v) { // res 0,(ix+nn)
	return resbyte(v, 0);
}
temp8 Oplx88(temp8 v) { // res 1,(ix+nn)
	return resbyte(v, 1);
}
temp8 Oplx90(temp8 v) { // res 2,(ix+nn)
	return resbyte(v, 2);
}
temp8 Oplx98(temp8 v) { // res 3,(ix+nn)
	return resbyte(v, 3);
}
temp8 OplxA0(temp8 v) { // res 4,(ix+nn)
	return resbyte(v, 4);
}
temp8 OplxA8(temp8 v) { // res 5,(ix+nn)
	return resbyte(v, 5);
}
temp8 OplxB0(temp8 v) { // res 6,(ix+nn)
	return resbyte(v, 6);
}
temp8 OplxB8(temp8 v) { // res 7,(ix+nn)
	return resbyte(v, 7);
}
temp8 OplxC0(temp8 v) { // set 0,(ix+nn)
	return setbyte(v, 0);
}
temp8 OplxC8(temp8 v) { // set 1,(ix+nn)
	return setbyte(v, 1);
}
temp8 OplxD0(temp8 v) { // set 2,(ix+nn)
	return setbyte(v, 2);
}
temp8 OplxD8(temp8 v) { // set 3,(ix+nn)
	return setbyte(v, 3);
}
temp8 OplxE0(temp8 v) { // set 4,(ix+nn)
	return setbyte(v, 4);
}
temp8 OplxE8(temp8 v) { // set 5,(ix+nn)
	return setbyte(v, 5);
}
temp8 OplxF0(temp8 v) { // set 6,(ix+nn)
	return setbyte(v, 6);
}
temp8 OplxF8(temp8 v) { // set 7,(ix+nn)
	return setbyte(v, 7);
}

#ifndef USE_Z80T
inline void DDFD(byte opcode)
{
	byte op1; // last DD/FD prefix
	do
	{
		op1 = opcode;
		opcode = Fetch();
	} while((opcode | 0x20) == 0xFD); // opcode == DD/FD

	if(opcode == 0xCB)
	{
		dword ptr; // pointer to DDCB operand
		ptr = ((op1 == 0xDD) ? ix : iy) + (signed char)ReadInc(pc);
		memptr = ptr;
		// DDCBnnXX,FDCBnnXX increment R by 2, not 3!
		opcode = ReadInc(pc);
		t += 4;
		byte v = (this->*logic_ix_opcodes[opcode])(Read(ptr));
		if((opcode & 0xC0) == 0x40)// bit n,rm
		{
			t += 8;
			return;
		}
		// select destination register for shift/res/set
		(this->*reg_offset[opcode & 7]) = v; // ???
		Write(ptr, v);
		t += 11;
		return;
	}
	if(opcode == 0xED)
	{
		OpED();
		return;
	}
	// one prefix: DD/FD
	op1 == 0xDD ? (this->*ix_opcodes[opcode])() : (this->*iy_opcodes[opcode])();
}
#endif
#ifndef USE_Z80T
inline void OpDD()
{
	DDFD(0xDD);
}
inline void OpFD()
{
	DDFD(0xFD);
}
#endif

#endif//__Z80_OP_DDCB_H__
