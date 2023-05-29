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

#ifndef	__Z80_OP_H__
#define	__Z80_OP_H__

#pragma once

void inc8(byte& x)
{
	f = incf[x] | (f & CF);
	x++;
}
void dec8(byte& x)
{
	f = decf[x] | (f & CF);
	x--;
}
void add8(byte src)
{
	f = adcf[a + src*0x100];
	a += src;
}
void adc8(byte src)
{
	byte carry = f & CF;
	f = adcf[a + src*0x100 + 0x10000*carry];
	a += src + carry;
}
void sub8(byte src)
{
	f = sbcf[a*0x100 + src];
	a -= src;
}
void sbc8(byte src)
{
	byte carry = f & CF;
	f = sbcf[a*0x100 + src + 0x10000*carry];
	a -= src + carry;
}
void and8(byte src)
{
	a &= src;
	f = log_f[a] | HF;
}
void or8(byte src)
{
	a |= src;
	f = log_f[a];
}
void xor8(byte src)
{
	a ^= src;
	f = log_f[a];
}
void cp8(byte src)
{
	f = cpf[a*0x100 + src];
}
void bit(byte src, byte bit)
{
	f = log_f[src & (1 << bit)] | HF | (f & CF) | (src & (F3|F5));
}
void rlc8(byte &x) {
	f = rlcf[x];
	x = rol[x];
}
void rrc8(byte& x) {
	f = rrcf[x];
	x = ror[x];
}
void rl8(byte &x) {
	if (f & CF)
		f = rl1[x], x = (x << 1) + 1;
	else
		f = rl0[x], x = (x << 1);
}
void rr8(byte& x) {
	if (f & CF)
		f = rr1[x], x = (x >> 1) + 0x80;
	else
		f = rr0[x], x = (x >> 1);
}
void sla8(byte& x) {
	f = rl0[x], x = (x << 1);
}
void sra8(byte& x) {
	f = sraf[x], x = (x >> 1) + (x & 0x80);
}
void sli8(byte& x) {
	f = rl1[x], x = (x << 1) + 1;
}
void srl8(byte& x) {
	f = rr0[x], x = (x >> 1);
}
void bitmem(byte src, byte bit)
{
	f = log_f[src & (1 << bit)] | HF | (f & CF);
	f = (f & ~(F3|F5)) | (mem_h & (F3|F5));
}
void res(byte& src, byte bit) const
{
	src &= ~(1 << bit);
}
byte resbyte(byte src, byte bit) const
{
	return src & ~(1 << bit);
}
void set(byte& src, byte bit) const
{
	src |= (1 << bit);
}
byte setbyte(byte src, byte bit) const
{
	return src | (1 << bit);
}
void add16(int& r1, int& r2) {
	memptr = r1+1;
	f = (f & ~(NF | CF | F5 | F3 | HF));
	f |= (((r1 & 0x0FFF) + (r2 & 0x0FFF)) >> 8) & 0x10; /* HF */
	r1 = (r1 & 0xFFFF) + (r2 & 0xFFFF);
	if (r1 & 0x10000) f |= CF;
	f |= ((r1>>8) & (F5 | F3));
	t += 7;
}
void adc16(int& reg) {
	memptr = hl+1;
	byte fl = (((hl & 0x0FFF) + (reg & 0x0FFF) + (af & CF)) >> 8) & 0x10; /* HF */
	unsigned tmp = (hl & 0xFFFF) + (reg & 0xFFFF) + (af & CF);
	if (tmp & 0x10000) fl |= CF;
	if (!(unsigned short)tmp) fl |= ZF;
	int ri = (int)(short)hl + (int)(short)reg + (int)(af & CF);
	if (ri < -0x8000 || ri >= 0x8000) fl |= PV;
	hl = tmp;
	f = fl | (h & (F3|F5|SF));
	t += 7;
}
// hl, reg
void sbc16(int& reg) {
	memptr = hl+1;
	byte fl = NF;
	fl |= (((hl & 0x0FFF) - (reg & 0x0FFF) - (af & CF)) >> 8) & 0x10; /* HF */
	unsigned tmp = (hl & 0xFFFF) - (reg & 0xFFFF) - (af & CF);
	if (tmp & 0x10000) fl |= CF;
	if (!(tmp & 0xFFFF)) fl |= ZF;
	int ri = (int)(short)hl - (int)(short)reg - (int)(af & CF);
	if (ri < -0x8000 || ri >= 0x8000) fl |= PV;
	hl = tmp;
	f = fl | (h & (F3|F5|SF));
	t += 7;
}
void push(word val) {
	sp -= 2;
	Write2(sp, val);
//    Write(--sp, val>>8);
//    Write(--sp, val&0xff);
}
#endif//__Z80_OP_H__
