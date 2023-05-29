/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2013 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
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

/* ED opcodes */

#ifndef	__Z80_OP_ED_H__
#define	__Z80_OP_ED_H__

#pragma once

void Ope40() { // in b,(c)
	t += 4;
	memptr = bc+1;
	b = IoRead(bc);
//    f = log_f[b] | (f & CF);
    set_logic_flags_preserve(b, CF);
}
void Ope41() { // out (c),b
	t += 4;
	memptr = bc+1;
	IoWrite(bc, b);
}
void Ope42() { // sbc hl,bc
    sbc16(bc);
}
void Ope43() { // ld (nnnn),bc
	temp16 adr = Read2Inc(pc);
	memptr = adr+1;
	Write2(adr, bc);
	t += 12;
}
#ifndef USE_Z80T
void Ope44() { // neg
    f = sbcf[a];
	a = -a;
}
#endif
void Ope45() { // retn
	iff1 = iff2;
	temp16 addr = Read2Inc(sp);
	pc = addr;
	memptr = addr;
	t += 6;
}
void Ope46() { // im 0
	im = 0;
}
void Ope47() { // ld i,a
	i = a;
	t++;
}
void Ope48() { // in c,(c)
	t += 4;
	memptr = bc+1;
	c = IoRead(bc);
//	f = log_f[c] | (f & CF);
    set_logic_flags_preserve(c, CF);
}
void Ope49() { // out (c),c
	t += 4;
	memptr = bc+1;
	IoWrite(bc, c);
}
void Ope4A() { // adc hl,bc
    adc16(bc);
}
void Ope4B() { // ld bc,(nnnn)
	temp16 adr = Read2Inc(pc);
	memptr = adr+1;
	bc = Read2(adr);
	t += 12;
}
void Ope4C() { Ope44(); } // neg
void Ope4D() { // reti
	iff1 = iff2;
	temp16 addr = Read2Inc(sp);
	pc = addr;
	memptr = addr;
	t += 6;
}
void Ope4E() { // im 0/1 -> im1
	im = 1;
}
void Ope4F() { // ld r,a
	r_low = a;
	r_hi = a & 0x80;
	t++;
}
void Ope50() { // in d,(c)
	t += 4;
	memptr = bc+1;
	d = IoRead(bc);
//    f = log_f[d] | (f & CF);
    set_logic_flags_preserve(d, CF);
}
void Ope51() { // out (c),d
	t += 4;
	memptr = bc+1;
	IoWrite(bc, d);
}
void Ope52() { // sbc hl,de
    sbc16(de);
}
void Ope53() { // ld (nnnn),de
	temp16 adr = Read2Inc(pc);
	memptr = adr+1;
	Write2(adr, de);
	t += 12;
}
void Ope54() { Ope44(); } // neg
void Ope55() { Ope45(); } // retn
void Ope56() { Ope4E(); } // im 1
#ifndef USE_Z80T
void Ope57() { // ld a,i
    a = i;
    f = (log_f[a] | (f & CF)) & ~PV;
    t++;
	if (iff1 && (t+10 < frame_tacts)) f |= PV;
}
#endif
void Ope58() { // in e,(c)
	t += 4;
	memptr = bc+1;
	e = IoRead(bc);
//    f = log_f[e] | (f & CF);
    set_logic_flags_preserve(e, CF);
}
void Ope59() { // out (c),e
	t += 4;
	memptr = bc+1;
	IoWrite(bc, e);
}
void Ope5A() { // adc hl,de
    adc16(de);
}
void Ope5B() { // ld de,(nnnn)
    temp16 adr = Read2Inc(pc);
	de = Read2(adr);
	t += 12;
}
void Ope5C() { Ope44(); } // neg
void Ope5D() { Ope4D(); } // reti
void Ope5E() { // im 2
	im = 2;
}
#ifndef USE_Z80T
void Ope5F() { // ld a,r
#ifdef NO_UPDATE_RLOW_IN_FETCH
    // save on counting r_low
    a = ((t>>2) & 0x7F) | r_hi;
#else
    a = (r_low & 0x7F) | r_hi;
#endif
	f = (log_f[a] | (f & CF)) & ~PV;
	if (iff2 && ((t+10 < frame_tacts) || eipos+8==t)) f |= PV;
	t++;
}
#endif
void Ope60() { // in h,(c)
	t += 4;
	memptr = bc+1;
	h = IoRead(bc);
//	f = log_f[h] | (f & CF);
    set_logic_flags_preserve(h, CF);
}
void Ope61() { // out (c),h
	t += 4;
	memptr = bc+1;
	IoWrite(bc, h);
}
void Ope62() { // sbc hl,hl
    sbc16(hl);
}
void Ope63() { Op22(); } // ld (nnnn),hl
void Ope64() { Ope44(); } // neg
void Ope65() { Ope45(); } // retn
void Ope66() { Ope46(); } // im 0
#ifndef USE_Z80T
void Ope67() { // rrd
    byte tmp = Read(hl);
	memptr = hl+1;
	Write(hl, (a << 4) | (tmp >> 4));
	a = (a & 0xF0) | (tmp & 0x0F);
	f = log_f[a] | (f & CF);
	t += 10;
}
#endif
void Ope68() { // in l,(c)
	t += 4;
	memptr = bc+1;
	l = IoRead(bc);
//    f = log_f[l] | (f & CF);
    set_logic_flags_preserve(l, CF);
}
void Ope69() { // out (c),l
	t += 4;
	memptr = bc+1;
	IoWrite(bc, l);
}
void Ope6A() { // adc hl,hl
    adc16(hl);
}
void Ope6B() { Op2A(); } // ld hl,(nnnn)
void Ope6C() { Ope44(); } // neg
void Ope6D() { Ope4D(); } // reti
void Ope6E() { Ope56(); } // im 0/1 -> im 1
#ifndef USE_Z80T
void Ope6F() { // rld
    byte tmp = Read(hl);
	memptr = hl+1;
	Write(hl, (a & 0x0F) | (tmp << 4));
	a = (a & 0xF0) | (tmp >> 4);
	f = log_f[a] | (f & CF);
	t += 10;
}
#endif
void Ope70() { // in (c)
    t += 4;
	memptr = bc+1;
//    f = log_f[IoRead(bc)] | (f & CF);
    set_logic_flags_preserve(IoRead(bc), CF);
}
void Ope71() { // out (c),0
	t += 4;
	memptr = bc+1;
	IoWrite(bc, 0);
}
void Ope72() { // sbc hl,sp
    sbc16(sp);
}
void Ope73() { // ld (nnnn),sp
	temp16 adr = Read2Inc(pc);
	memptr = adr+1;
	Write2(adr, sp);
	t += 12;
}
void Ope74() { Ope44(); } // neg
void Ope75() { Ope45(); } // retn
void Ope76() { // im 1
	im = 1;
}
void Ope77() { Op00(); } // nop
void Ope78() { // in a,(c)
	t += 4;
	memptr = bc+1;
	a = IoRead(bc);
    // f = log_f[a] | (f & CF);
    set_logic_flags_preserve(a, CF);
}
void Ope79() { // out (c),a
	t += 4;
	memptr = bc+1;
	IoWrite(bc, a);
}
void Ope7A() { // adc hl,sp
    adc16(sp);
}
void Ope7B() { // ld sp,(nnnn)
	temp16 adr = Read2Inc(pc);
	memptr = adr+1;
	sp = Read2(adr);
	t += 12;
}
void Ope7C() { Ope44(); } // neg
void Ope7D() { Ope4D(); } // reti
void Ope7E() { Ope5E(); } // im 2
void Ope7F() { Op00(); } // nop
#ifndef USE_Z80T
void OpeA0() { // ldi
    t += 8;
	temp8 tempbyte = ReadInc(hl);
	Write(de++, tempbyte);
	tempbyte += a; tempbyte = (tempbyte & F3) + ((tempbyte << 4) & F5);
	f = (f & ~(NF|HF|PV|F3|F5)) + tempbyte;
	if (--bc & 0xFFFF) f |= PV; //???
}
#endif
#ifndef USE_Z80T
void OpeA1() { // cpi
    t += 8;
    byte cf = f & CF;
    byte tempbyte = ReadInc(hl);
    f = cpf8b[a*0x100 + tempbyte] + cf;
    if (--bc & 0xFFFF) f |= PV; //???
    memptr++;
}
#endif
#ifndef USE_Z80T
void OpeA2A3AAABFlags(byte val, byte tmp)
{
	f = log_f[b] & ~PV;
	if(log_f[(tmp & 0x07) ^ b] & PV) f |= PV;
	if(tmp < val) f |= (HF|CF);
	if(val & 0x80) f |= NF;
}
#endif
void OpeA2() { // ini
	memptr = bc+1;
	t += 8;
	temp8 val = IoRead(bc);
	--b;
	Write(hl, val);
	hl++;
	OpeA2A3AAABFlags(val, val + c + 1);
}
void OpeA3() { // outi
    t += 8;
	temp8 val = ReadInc(hl);
	--b;
	IoWrite(bc, val);
	OpeA2A3AAABFlags(val, val + l);
	memptr = bc+1;
}
#ifndef USE_Z80T
void OpeA8() { // ldd
    t += 8;
	temp8 tempbyte = Read(hl--);
	Write(de--, tempbyte);
	tempbyte += a; tempbyte = (tempbyte & F3) + ((tempbyte << 4) & F5);
	f = (f & ~(NF|HF|PV|F3|F5)) + tempbyte;
	if (--bc & 0xFFFF) f |= PV; //???
}
#endif
#ifndef USE_Z80T
void OpeA9() { // cpd
    t += 8;
    byte cf = f & CF;
    byte tempbyte = Read(hl--);
    f = cpf8b[a*0x100 + tempbyte] + cf;
    if (--bc & 0xFFFF) f |= PV; //???
    memptr--;
}
#endif
void OpeAA() { // ind
    memptr = bc-1;
	t += 8;
	temp8 val = IoRead(bc);
	--b;
	Write(hl, val);
	--hl;
	OpeA2A3AAABFlags(val, val + c - 1);
}
void OpeAB() { // outd
    t += 8;
	temp8 val = Read(hl);
	--hl;
	--b;
	IoWrite(bc, val);
	OpeA2A3AAABFlags(val, val + l);
	memptr = bc-1;
}
void OpeB0() { // ldir
    OpeA0();
    if_flag_set(PV, [&] {
        pc -= 2, t += 5, memptr = pc+1; //???
    });
}
void OpeB1() { // cpir
    OpeA1();
    if_flag_set(PV, [&] {
        return if_flag_clear(ZF,
            [&] {
            pc -= 2, t += 5, memptr = pc+1;
        });
    });
}
void OpeB2() { // inir
    t += 8;
	memptr = bc+1;
	Write(hl, IoRead(bc));
	hl++;
	dec8(b);

	if_nonzero(b, [&] {
        f |= PV, pc -= 2, t += 5;
	}, [&] {
	    f &= ~PV;
	});
}
void OpeB3() { // otir
    t += 8;
	dec8(b);
	IoWrite(bc, Read(hl));
	hl++;
    memptr = bc+1;
    return if_nonzero(b, [&] {
        f |= PV, pc -= 2, t += 5;
        f &= ~CF;
        return if_zero(l, [&] {
           f |= CF;
        });
    }, [&] {
        f &= ~PV;
        return if_zero(l, [&] {
            f |= CF;
        });
    });
}
void OpeB8() { // lddr
    OpeA8();
    return if_flag_set(PV, [&] {
        pc -= 2, t += 5, memptr = pc+1; //???
    });
}
void OpeB9() { // cpdr
    OpeA9();
    return if_flag_set(PV, [&] {
          return if_flag_clear(ZF,
                               [&] {
                                   pc -= 2, t += 5, memptr = pc+1;
                               });
      });
}

void OpeBA() { // indr
    t += 8;
	memptr = bc-1;
	Write(hl, IoRead(bc));
	--hl;
	dec8(b);
    if_nonzero(b, [&] {
        f |= PV, pc -= 2, t += 5;
    }, [&] {
        f &= ~PV;
    });
}
void OpeBB() { // otdr
    t += 8;
    dec8(b);
    IoWrite(bc, Read(hl));
    --hl;
    memptr = bc-1;
    return if_nonzero(b, [&] {
        f |= PV, pc -= 2, t += 5;
        f &= ~CF;
        return if_equal(l, 255, [&] {
            f |= CF;
        });
    }, [&] {
        f &= ~PV;
        return if_equal(l, 255, [&] {
            f |= CF;
        });
    });
}

#ifndef USE_Z80T
void OpED()
{
	byte opcode = Fetch();
	(this->*ext_opcodes[opcode])();
}
#endif

#endif//__Z80_OP_ED_H__
