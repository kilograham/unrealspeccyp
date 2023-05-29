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

/* CB opcodes */

#ifndef	__Z80_OP_CB_H__
#define	__Z80_OP_CB_H__

#pragma once

void Opl00() { // rlc b
	rlc8(b);
}
void Opl01() { // rlc c
	rlc8(c);
}
void Opl02() { // rlc d
	rlc8(d);
}
void Opl03() { // rlc e
	rlc8(e);
}
void Opl04() { // rlc h
	rlc8(h);
}
void Opl05() { // rlc l
	rlc8(l);
}
void Opl06() { // rlc (hl)
	temp8 v = Read(hl);
	rlc8(v);
	Write(hl, v);
	t += 7;
}
void Opl07() { // rlc a
	rlc8(a);
}
void Opl08() { // rrc b
	rrc8(b);
}
void Opl09() { // rrc c
	rrc8(c);
}
void Opl0A() { // rrc d
	rrc8(d);
}
void Opl0B() { // rrc e
	rrc8(e);
}
void Opl0C() { // rrc h
	rrc8(h);
}
void Opl0D() { // rrc l
	rrc8(l);
}
void Opl0E() { // rrc (hl)
	temp8 v = Read(hl);
	rrc8(v);
	Write(hl, v);
	t += 7;
}
void Opl0F() { // rrc a
	rrc8(a);
}
void Opl10() { // rl b
	rl8(b);
}
void Opl11() { // rl c
	rl8(c);
}
void Opl12() { // rl d
	rl8(d);
}
void Opl13() { // rl e
	rl8(e);
}
void Opl14() { // rl h
	rl8(h);
}
void Opl15() { // rl l
	rl8(l);
}
void Opl16() { // rl (hl)
	temp8 v = Read(hl);
	rl8(v);
	Write(hl, v);
	t += 7;
}
void Opl17() { // rl a
	rl8(a);
}
void Opl18() { // rr b
	rr8(b);
}
void Opl19() { // rr c
	rr8(c);
}
void Opl1A() { // rr d
	rr8(d);
}
void Opl1B() { // rr e
	rr8(e);
}
void Opl1C() { // rr h
	rr8(h);
}
void Opl1D() { // rr l
	rr8(l);
}
void Opl1E() { // rr (hl)
	temp8 v = Read(hl);
	rr8(v);
	Write(hl, v);
	t += 7;
}
void Opl1F() { // rr a
	rr8(a);
}
void Opl20() { // sla b
	sla8(b);
}
void Opl21() { // sla c
	sla8(c);
}
void Opl22() { // sla d
	sla8(d);
}
void Opl23() { // sla e
	sla8(e);
}
void Opl24() { // sla h
	sla8(h);
}
void Opl25() { // sla l
	sla8(l);
}
void Opl26() { // sla (hl)
	temp8 v = Read(hl);
	sla8(v);
	Write(hl, v);
	t += 7;
}
void Opl27() { // sla a
	sla8(a);
}
void Opl28() { // sra b
	sra8(b);
}
void Opl29() { // sra c
	sra8(c);
}
void Opl2A() { // sra d
	sra8(d);
}
void Opl2B() { // sra e
	sra8(e);
}
void Opl2C() { // sra h
	sra8(h);
}
void Opl2D() { // sra l
	sra8(l);
}
void Opl2E() { // sra (hl)
	temp8 v = Read(hl);
	sra8(v);
	Write(hl, v);
	t += 7;
}
void Opl2F() { // sra a
	sra8(a);
}
void Opl30() { // sli b
	sli8(b);
}
void Opl31() { // sli c
	sli8(c);
}
void Opl32() { // sli d
	sli8(d);
}
void Opl33() { // sli e
	sli8(e);
}
void Opl34() { // sli h
	sli8(h);
}
void Opl35() { // sli l
	sli8(l);
}
void Opl36() { // sli (hl)
	temp8 v = Read(hl);
	sli8(v);
	Write(hl, v);
	t += 7;
}
void Opl37() { // sli a
	sli8(a);
}
void Opl38() { // srl b
	srl8(b);
}
void Opl39() { // srl c
	srl8(c);
}
void Opl3A() { // srl d
	srl8(d);
}
void Opl3B() { // srl e
	srl8(e);
}
void Opl3C() { // srl h
	srl8(h);
}
void Opl3D() { // srl l
	srl8(l);
}
void Opl3E() { // srl (hl)
	temp8 v = Read(hl);
	srl8(v);
	Write(hl, v);
	t += 7;
}
void Opl3F() { // srl a
	srl8(a);
}
void Opl40() { // bit 0,b
	bit(b, 0);
}
void Opl41() { // bit 0,c
	bit(c, 0);
}
void Opl42() { // bit 0,d
	bit(d, 0);
}
void Opl43() { // bit 0,e
	bit(e, 0);
}
void Opl44() { // bit 0,h
	bit(h, 0);
}
void Opl45() { // bit 0,l
	bit(l, 0);
}
void Opl46() { // bit 0,(hl)
	bitmem(Read(hl), 0);
	t += 4;
}
void Opl47() { // bit 0,a
	bit(a, 0);
}
void Opl48() { // bit 1,b
	bit(b, 1);
}
void Opl49() { // bit 1,c
	bit(c, 1);
}
void Opl4A() { // bit 1,d
	bit(d, 1);
}
void Opl4B() { // bit 1,e
	bit(e, 1);
}
void Opl4C() { // bit 1,h
	bit(h, 1);
}
void Opl4D() { // bit 1,l
	bit(l, 1);
}
void Opl4E() { // bit 1,(hl)
	bitmem(Read(hl), 1);
	t += 4;
}
void Opl4F() { // bit 1,a
	bit(a, 1);
}
void Opl50() { // bit 2,b
	bit(b, 2);
}
void Opl51() { // bit 2,c
	bit(c, 2);
}
void Opl52() { // bit 2,d
	bit(d, 2);
}
void Opl53() { // bit 2,e
	bit(e, 2);
}
void Opl54() { // bit 2,h
	bit(h, 2);
}
void Opl55() { // bit 2,l
	bit(l, 2);
}
void Opl56() { // bit 2,(hl)
	bitmem(Read(hl), 2);
	t += 4;
}
void Opl57() { // bit 2,a
	bit(a, 2);
}
void Opl58() { // bit 3,b
	bit(b, 3);
}
void Opl59() { // bit 3,c
	bit(c, 3);
}
void Opl5A() { // bit 3,d
	bit(d, 3);
}
void Opl5B() { // bit 3,e
	bit(e, 3);
}
void Opl5C() { // bit 3,h
	bit(h, 3);
}
void Opl5D() { // bit 3,l
	bit(l, 3);
}
void Opl5E() { // bit 3,(hl)
	bitmem(Read(hl), 3);
	t += 4;
}
void Opl5F() { // bit 3,a
	bit(a, 3);
}
void Opl60() { // bit 4,b
	bit(b, 4);
}
void Opl61() { // bit 4,c
	bit(c, 4);
}
void Opl62() { // bit 4,d
	bit(d, 4);
}
void Opl63() { // bit 4,e
	bit(e, 4);
}
void Opl64() { // bit 4,h
	bit(h, 4);
}
void Opl65() { // bit 4,l
	bit(l, 4);
}
void Opl66() { // bit 4,(hl)
	bitmem(Read(hl), 4);
	t += 4;
}
void Opl67() { // bit 4,a
	bit(a, 4);
}
void Opl68() { // bit 5,b
	bit(b, 5);
}
void Opl69() { // bit 5,c
	bit(c, 5);
}
void Opl6A() { // bit 5,d
	bit(d, 5);
}
void Opl6B() { // bit 5,e
	bit(e, 5);
}
void Opl6C() { // bit 5,h
	bit(h, 5);
}
void Opl6D() { // bit 5,l
	bit(l, 5);
}
void Opl6E() { // bit 5,(hl)
	bitmem(Read(hl), 5);
	t += 4;
}
void Opl6F() { // bit 5,a
	bit(a, 5);
}
void Opl70() { // bit 6,b
	bit(b, 6);
}
void Opl71() { // bit 6,c
	bit(c, 6);
}
void Opl72() { // bit 6,d
	bit(d, 6);
}
void Opl73() { // bit 6,e
	bit(e, 6);
}
void Opl74() { // bit 6,h
	bit(h, 6);
}
void Opl75() { // bit 6,l
	bit(l, 6);
}
void Opl76() { // bit 6,(hl)
	bitmem(Read(hl), 6);
	t += 4;
}
void Opl77() { // bit 6,a
	bit(a, 6);
}
void Opl78() { // bit 7,b
	bit(b, 7);
}
void Opl79() { // bit 7,c
	bit(c, 7);
}
void Opl7A() { // bit 7,d
	bit(d, 7);
}
void Opl7B() { // bit 7,e
	bit(e, 7);
}
void Opl7C() { // bit 7,h
	bit(h, 7);
}
void Opl7D() { // bit 7,l
	bit(l, 7);
}
void Opl7E() { // bit 7,(hl)
	bitmem(Read(hl), 7);
	t += 4;
}
void Opl7F() { // bit 7,a
	bit(a, 7);
}
void Opl80() { // res 0,b
	res(b, 0);
}
void Opl81() { // res 0,c
	res(c, 0);
}
void Opl82() { // res 0,d
	res(d, 0);
}
void Opl83() { // res 0,e
	res(e, 0);
}
void Opl84() { // res 0,h
	res(h, 0);
}
void Opl85() { // res 0,l
	res(l, 0);
}
void Opl86() { // res 0,(hl)
	temp8 v = Read(hl); res(v, 0); Write(hl, v);
	t += 7;
}
void Opl87() { // res 0,a
	res(a, 0);
}
void Opl88() { // res 1,b
	res(b, 1);
}
void Opl89() { // res 1,c
	res(c, 1);
}
void Opl8A() { // res 1,d
	res(d, 1);
}
void Opl8B() { // res 1,e
	res(e, 1);
}
void Opl8C() { // res 1,h
	res(h, 1);
}
void Opl8D() { // res 1,l
	res(l, 1);
}
void Opl8E() { // res 1,(hl)
	temp8 v = Read(hl); res(v, 1); Write(hl, v);
	t += 7;
}
void Opl8F() { // res 1,a
	res(a, 1);
}
void Opl90() { // res 2,b
	res(b, 2);
}
void Opl91() { // res 2,c
	res(c, 2);
}
void Opl92() { // res 2,d
	res(d, 2);
}
void Opl93() { // res 2,e
	res(e, 2);
}
void Opl94() { // res 2,h
	res(h, 2);
}
void Opl95() { // res 2,l
	res(l, 2);
}
void Opl96() { // res 2,(hl)
	temp8 v = Read(hl); res(v, 2); Write(hl, v);
	t += 7;
}
void Opl97() { // res 2,a
	res(a, 2);
}
void Opl98() { // res 3,b
	res(b, 3);
}
void Opl99() { // res 3,c
	res(c, 3);
}
void Opl9A() { // res 3,d
	res(d, 3);
}
void Opl9B() { // res 3,e
	res(e, 3);
}
void Opl9C() { // res 3,h
	res(h, 3);
}
void Opl9D() { // res 3,l
	res(l, 3);
}
void Opl9E() { // res 3,(hl)
	temp8 v = Read(hl); res(v, 3); Write(hl, v);
	t += 7;
}
void Opl9F() { // res 3,a
	res(a, 3);
}
void OplA0() { // res 4,b
	res(b, 4);
}
void OplA1() { // res 4,c
	res(c, 4);
}
void OplA2() { // res 4,d
	res(d, 4);
}
void OplA3() { // res 4,e
	res(e, 4);
}
void OplA4() { // res 4,h
	res(h, 4);
}
void OplA5() { // res 4,l
	res(l, 4);
}
void OplA6() { // res 4,(hl)
	temp8 v = Read(hl); res(v, 4); Write(hl, v);
	t += 7;
}
void OplA7() { // res 4,a
	res(a, 4);
}
void OplA8() { // res 5,b
	res(b, 5);
}
void OplA9() { // res 5,c
	res(c, 5);
}
void OplAA() { // res 5,d
	res(d, 5);
}
void OplAB() { // res 5,e
	res(e, 5);
}
void OplAC() { // res 5,h
	res(h, 5);
}
void OplAD() { // res 5,l
	res(l, 5);
}
void OplAE() { // res 5,(hl)
	temp8 v = Read(hl); res(v, 5); Write(hl, v);
	t += 7;
}
void OplAF() { // res 5,a
	res(a, 5);
}
void OplB0() { // res 6,b
	res(b, 6);
}
void OplB1() { // res 6,c
	res(c, 6);
}
void OplB2() { // res 6,d
	res(d, 6);
}
void OplB3() { // res 6,e
	res(e, 6);
}
void OplB4() { // res 6,h
	res(h, 6);
}
void OplB5() { // res 6,l
	res(l, 6);
}
void OplB6() { // res 6,(hl)
	temp8 v = Read(hl); res(v, 6); Write(hl, v);
	t += 7;
}
void OplB7() { // res 6,a
	res(a, 6);
}
void OplB8() { // res 7,b
	res(b, 7);
}
void OplB9() { // res 7,c
	res(c, 7);
}
void OplBA() { // res 7,d
	res(d, 7);
}
void OplBB() { // res 7,e
	res(e, 7);
}
void OplBC() { // res 7,h
	res(h, 7);
}
void OplBD() { // res 7,l
	res(l, 7);
}
void OplBE() { // res 7,(hl)
	temp8 v = Read(hl); res(v, 7); Write(hl, v);
	t += 7;
}
void OplBF() { // res 7,a
	res(a, 7);
}
void OplC0() { // set 0,b
	set(b, 0);
}
void OplC1() { // set 0,c
	set(c, 0);
}
void OplC2() { // set 0,d
	set(d, 0);
}
void OplC3() { // set 0,e
	set(e, 0);
}
void OplC4() { // set 0,h
	set(h, 0);
}
void OplC5() { // set 0,l
	set(l, 0);
}
void OplC6() { // set 0,(hl)
	temp8 v = Read(hl); set(v, 0); Write(hl, v);
	t += 7;
}
void OplC7() { // set 0,a
	set(a, 0);
}
void OplC8() { // set 1,b
	set(b, 1);
}
void OplC9() { // set 1,c
	set(c, 1);
}
void OplCA() { // set 1,d
	set(d, 1);
}
void OplCB() { // set 1,e
	set(e, 1);
}
void OplCC() { // set 1,h
	set(h, 1);
}
void OplCD() { // set 1,l
	set(l, 1);
}
void OplCE() { // set 1,(hl)
	temp8 v = Read(hl); set(v, 1); Write(hl, v);
	t += 7;
}
void OplCF() { // set 1,a
	set(a, 1);
}
void OplD0() { // set 2,b
	set(b, 2);
}
void OplD1() { // set 2,c
	set(c, 2);
}
void OplD2() { // set 2,d
	set(d, 2);
}
void OplD3() { // set 2,e
	set(e, 2);
}
void OplD4() { // set 2,h
	set(h, 2);
}
void OplD5() { // set 2,l
	set(l, 2);
}
void OplD6() { // set 2,(hl)
	temp8 v = Read(hl); set(v, 2); Write(hl, v);
	t += 7;
}
void OplD7() { // set 2,a
	set(a, 2);
}
void OplD8() { // set 3,b
	set(b, 3);
}
void OplD9() { // set 3,c
	set(c, 3);
}
void OplDA() { // set 3,d
	set(d, 3);
}
void OplDB() { // set 3,e
	set(e, 3);
}
void OplDC() { // set 3,h
	set(h, 3);
}
void OplDD() { // set 3,l
	set(l, 3);
}
void OplDE() { // set 3,(hl)
	temp8 v = Read(hl); set(v, 3); Write(hl, v);
	t += 7;
}
void OplDF() { // set 3,a
	set(a, 3);
}
void OplE0() { // set 4,b
	set(b, 4);
}
void OplE1() { // set 4,c
	set(c, 4);
}
void OplE2() { // set 4,d
	set(d, 4);
}
void OplE3() { // set 4,e
	set(e, 4);
}
void OplE4() { // set 4,h
	set(h, 4);
}
void OplE5() { // set 4,l
	set(l, 4);
}
void OplE6() { // set 4,(hl)
	temp8 v = Read(hl); set(v, 4); Write(hl, v);
	t += 7;
}
void OplE7() { // set 4,a
	set(a, 4);
}
void OplE8() { // set 5,b
	set(b, 5);
}
void OplE9() { // set 5,c
	set(c, 5);
}
void OplEA() { // set 5,d
	set(d, 5);
}
void OplEB() { // set 5,e
	set(e, 5);
}
void OplEC() { // set 5,h
	set(h, 5);
}
void OplED() { // set 5,l
	set(l, 5);
}
void OplEE() { // set 5,(hl)
	temp8 v = Read(hl); set(v, 5); Write(hl, v);
	t += 7;
}
void OplEF() { // set 5,a
	set(a, 5);
}
void OplF0() { // set 6,b
	set(b, 6);
}
void OplF1() { // set 6,c
	set(c, 6);
}
void OplF2() { // set 6,d
	set(d, 6);
}
void OplF3() { // set 6,e
	set(e, 6);
}
void OplF4() { // set 6,h
	set(h, 6);
}
void OplF5() { // set 6,l
	set(l, 6);
}
void OplF6() { // set 6,(hl)
	temp8 v = Read(hl); set(v, 6); Write(hl, v);
	t += 7;
}
void OplF7() { // set 6,a
	set(a, 6);
}
void OplF8() { // set 7,b
	set(b, 7);
}
void OplF9() { // set 7,c
	set(c, 7);
}
void OplFA() { // set 7,d
	set(d, 7);
}
void OplFB() { // set 7,e
	set(e, 7);
}
void OplFC() { // set 7,h
	set(h, 7);
}
void OplFD() { // set 7,l
	set(l, 7);
}
void OplFE() { // set 7,(hl)
	temp8 v = Read(hl); set(v, 7); Write(hl, v);
	t += 7;
}
void OplFF() { // set 7,a
	set(a, 7);
}

#ifndef USE_Z80T
inline void OpCB()
{
	byte opcode = Fetch();
	(this->*logic_opcodes[opcode])();
}
#endif

#endif//__Z80_OP_CB_H__
