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
#include "../z80/z80.h"
#include "../devices/memory.h"
#include "../devices/ula.h"
#ifndef NO_USE_AY
#include "../devices/sound/ay.h"
#endif
#include "../speccy.h"
#include "../platform/endian.h"
#include "../platform/platform.h"

#include "snapshot.h"

namespace xSnapshot
{

#pragma pack(push, 1)
#ifndef NO_USE_SNA
struct eSnapshot_SNA_header
{
	byte i;
	word alt_hl, alt_de, alt_bc, alt_af;
	word hl, de, bc, iy, ix;
	byte iff1; // 00 - reset, FF - set
	byte r;
	word af, sp;
	byte im, pFE;
};

struct eSnaphsot_SNA128_header {
	// 128k extension
	word pc;
#ifndef NO_USE_128K
	byte p7FFD;
	byte trdos;
#endif
};

struct eSnapshot_SNA {
	struct eSnapshot_SNA_header header;
	byte page5[eMemory::PAGE_SIZE];    // 4000-7FFF
	byte page2[eMemory::PAGE_SIZE];    // 8000-BFFF
	byte page[eMemory::PAGE_SIZE];    // C000-FFFF
	struct eSnaphsot_SNA128_header header_128;
#ifndef NO_USE_128K
	byte pages[6 * eMemory::PAGE_SIZE]; // all other pages (can contain duplicated page 5 or 2)
#endif
	enum { S_48 = 49179,
#ifndef NO_USE_128K
		S_128_5 = 131103, S_128_6 = 147487
#endif
	};
};
#endif

struct eSnapshot_Z80_v1
{
	byte a,f;
	word bc,hl,pc,sp;
	byte i,r,flags;
	word de,bc1,de1,hl1;
	byte a1,f1;
	word iy,ix;
	byte iff1, iff2, im;
};

struct eSnapshot_Z80_v2
{
	word len, newpc;
	byte model, p7FFD;
	byte r1,r2, p7FFD_1;
	byte AY[16];
};

struct eSnapshot_Z80
{
	eSnapshot_Z80_v1 v1;
	/* 2.01 extension */
	eSnapshot_Z80_v2 v2;
	/* 3.0 extension */
};
#pragma pack(pop)

struct eZ80Accessor : public xZ80::eZ80
{
#ifndef USE_STREAM
#ifndef NO_USE_SNA
	bool SetState(const eSnapshot_SNA* s, size_t buf_size);
	size_t StoreState(eSnapshot_SNA* s);
#endif
	bool SetState(const eSnapshot_Z80* s, size_t buf_size);
#else
#ifndef NO_USE_SNA
	bool SetStateSNA(struct stream *stream);
	size_t StoreState(eSnapshot_SNA* s);
#endif
	bool SetStateZ80(struct stream *stream);
#endif
#ifndef USE_STREAM
	void UnpackPage(byte* dst, int dstlen, const byte* src, int srclen);
#else
	void UnpackPage(byte* dst, int dstlen, struct stream *stream, int srclen);
#endif
	void SetupDevices(bool model48k)
	{
#ifndef NO_USE_128K
		devices->Get<eRom>()->Mode48k(model48k);
		devices->Get<eRam>()->Mode48k(model48k);
		devices->Get<eUla>()->Mode48k(model48k);
#else
		assert(model48k);
#endif
		devices->Init();
	}
};
#ifndef NO_USE_SNA
#ifndef USE_STREAM
bool eZ80Accessor::SetState(const eSnapshot_SNA* sna, size_t buf_size)
{
	const eSnapshot_SNA_header *s = &sna->header;
#else
bool eZ80Accessor::SetStateSNA(struct stream *stream)
{
	eSnapshot_SNA_header snap;
	stream_read(stream, (uint8_t *)&snap, sizeof(eSnapshot_SNA_header), true);
	size_t buf_size = stream->size;
	const eSnapshot_SNA_header *s = &snap;
#endif
	bool sna48 = (buf_size == eSnapshot_SNA::S_48);
#ifndef NO_USE_128K
	bool sna128 = (buf_size == eSnapshot_SNA::S_128_5) || (buf_size == eSnapshot_SNA::S_128_6);
	if(!sna48 && !sna128)
		return false;
#else
	if (!sna48) return false;
#endif

	SetupDevices(sna48);
#ifndef USE_Z80_ARM
	alt.af = SwapWord(s->alt_af);
	alt.bc = SwapWord(s->alt_bc);
	alt.de = SwapWord(s->alt_de);
	alt.hl = SwapWord(s->alt_hl);
	af = SwapWord(s->af);
	bc = SwapWord(s->bc);
	de = SwapWord(s->de);
	hl = SwapWord(s->hl);
	ix = SwapWord(s->ix);
	iy = SwapWord(s->iy);
	sp = SwapWord(s->sp);
	i = s->i;
	r_low = s->r;
	r_hi = s->r & 0x80;
	im = s->im;
	iff1 = s->iff1 ? 1 : 0;
	devices->IoWrite(0xfe, s->pFE, t);
#else
	auto &rs = z80a_resting_state;
	rs.alt_af = SwapWord(s->alt_af);
	rs.alt_bc = SwapWord(s->alt_bc);
	rs.alt_de = SwapWord(s->alt_de);
	rs.alt_hl = SwapWord(s->alt_hl);
	rs.af = SwapWord(s->af);
	rs.bc = SwapWord(s->bc);
	rs.de = SwapWord(s->de);
	rs.hl = SwapWord(s->hl);
	rs.ix = SwapWord(s->ix);
	rs.iy = SwapWord(s->iy);
	rs.sp = SwapWord(s->sp);
	rs.i = s->i;
	rs.r_low = s->r;
	rs.r_hi = s->r & 0x80;
	rs.im = s->im;
	rs.iff1 = s->iff1 ? 1 : 0;
	devices->IoWrite(0xfe, s->pFE, rs.t);
#endif

	int p_size = eMemory::PAGE_SIZE;

#ifndef USE_STREAM
#ifndef NO_USE_128K
	uint p = sna48 ? 0 : (s128->p7FFD & 7u);
#else
	uint p = 0;
#endif
	memcpy(memory->Get(eMemory::P_RAM5), sna->page5, p_size);
	memcpy(memory->Get(eMemory::P_RAM2), sna->page2, p_size);
	memcpy(memory->Get(eMemory::P_RAM0 + p), sna->page, p_size);
#else
	stream_read(stream, memory->Get(eMemory::P_RAM5), p_size, true);
	stream_read(stream, memory->Get(eMemory::P_RAM2), p_size, true);
	stream_read(stream, memory->Get(eMemory::P_RAM0), p_size, true);
#endif

	if(sna48)
	{
#ifndef USE_Z80_ARM
		pc = memory->Read(sp) + 0x100 * memory->Read(sp+1);
		sp += 2;
#else
		rs.pc = memory->Read(rs.sp) + 0x100 * memory->Read(rs.sp+1);
		rs.sp += 2;
#endif
		devices->Get<eRom>()->SelectPage(eRom::ROM_48);
		return true;
	}

#ifndef USE_STREAM
	const eSnaphsot_SNA128_header *s128 = &sna->header_128;
#else
	eSnaphsot_SNA128_header snap128;
	stream_read(stream, (uint8_t *)&snap128, sizeof(snap128), true);
#ifndef NO_USE_128K
	const eSnaphsot_SNA128_header *s128 = &snap128;
#endif
#endif

#ifndef NO_USE_128K
#ifndef USE_Z80_ARM
	pc = SwapWord(s128->pc);
	devices->IoWrite(0x7ffd, s128->p7FFD, t);
#else
	rs.pc = SwapWord(s128->pc);
	devices->IoWrite(0x7ffd, s128->p7FFD, rs.t);
#endif
#ifndef NO_USE_DOS
	devices->Get<eRom>()->SelectPage(s128->trdos ? eRom::ROM_DOS : eRom::ROM_128_0);
#else
	assert(!s128->trdos); // todo error
	devices->Get<eRom>()->SelectPage(eRom::ROM_128_0);
#endif
	byte mapped = 0x24 | (1 << (s128->p7FFD & 7));
#ifndef USE_STREAM
	const byte* page = sna->pages;
	for(int i = 0; i < 8; ++i)
	{
		if(!(mapped & (1 << i)))
		{
			memcpy(memory->Get(eMemory::P_RAM0 + i), page, p_size);
			page += p_size;
		}
	}
#else
	for(int i = 0; i < 8; ++i)
	{
		if(!(mapped & (1 << i)))
		{
			stream_read(stream, memory->Get(eMemory::P_RAM0 + i), p_size, true);
		}
	}
#endif
#endif
	return true;
}
#ifndef NO_USE_SAVE
size_t eZ80Accessor::StoreState(eSnapshot_SNA* s)
{
	s->trdos = devices->Get<eRom>()->DosSelected();
	s->alt_af = alt.af; s->alt_bc = alt.bc;
	s->alt_de = alt.de; s->alt_hl = alt.hl;
	s->af = af; s->bc = bc; s->de = de; s->hl = hl;
	s->ix = ix; s->iy = iy; s->sp = sp; s->pc = pc;
	s->i = i; s->r = (r_low & 0x7F)+r_hi; s->im = im;
	s->iff1 = iff1 ? 0xFF : 0;

	SwapEndian(s->alt_af);
	SwapEndian(s->alt_bc);
	SwapEndian(s->alt_de);
	SwapEndian(s->alt_hl);
	SwapEndian(s->af);
	SwapEndian(s->bc);
	SwapEndian(s->de);
	SwapEndian(s->hl);
	SwapEndian(s->ix);
	SwapEndian(s->iy);
	SwapEndian(s->sp);
	SwapEndian(s->pc);

	byte p7FFD = memory->Page(3) - eMemory::P_RAM0;
	if(!devices->Get<eUla>()->FirstScreen())
		p7FFD |= 0x08;
	byte pFE = devices->Get<eUla>()->BorderColor();
	s->p7FFD = p7FFD;
	s->pFE = pFE;
	byte mapped = 0x24 | (1 << (p7FFD & 7));
	if(devices->Get<eRam>()->Mode48k())
	{
		mapped = 0xff;
		s->sp -= 2;
		memory->Write(s->sp, pc_l);
		memory->Write(s->sp + 1, pc_h);
	}
	memcpy(s->page5, memory->Get(eMemory::P_RAM5), eMemory::PAGE_SIZE);
	memcpy(s->page2, memory->Get(eMemory::P_RAM2), eMemory::PAGE_SIZE);
	memcpy(s->page,  memory->Get(eMemory::P_RAM0 + (p7FFD & 7)), eMemory::PAGE_SIZE);
	byte* page = s->pages;
	int stored_128_pages = 0;
	for(byte i = 0; i < 8; i++)
	{
		if(!(mapped & (1 << i)))
		{
			memcpy(page, memory->Get(eMemory::P_RAM0 + i), eMemory::PAGE_SIZE);
			page += eMemory::PAGE_SIZE;
			++stored_128_pages;
		}
	}
	switch(stored_128_pages)
	{
	case 0:
		return eSnapshot_SNA::S_48;
	case 6:
		return eSnapshot_SNA::S_128_6;
	}
	return eSnapshot_SNA::S_128_5;
}
#endif
#endif
#ifndef USE_STREAM
bool eZ80Accessor::SetState(const eSnapshot_Z80* snap, size_t buf_size)
{
	const struct eSnapshot_Z80_v1 *s = &snap->v1;
#else
bool eZ80Accessor::SetStateZ80(struct stream *stream)
{
	eSnapshot_Z80_v1 snap;
	static_assert(sizeof(snap) == 30, "");
	stream_read(stream, (uint8_t *)&snap, sizeof(snap), true);
	const eSnapshot_Z80_v1 *s = &snap;
	size_t buf_size = stream->size;
#endif
	bool model48k = true;
	byte flags = s->flags;
	if(flags == 0xFF)
		flags = 1;
	const byte* ptr = (byte*)s + 30;
	word reg_pc = s->pc;
	int val7ffd = 0x30;
	eSnapshot_Z80_v2 *s2 = 0;
	if(reg_pc == 0)
	{ // 2.01
#ifndef USE_STREAM
		s2 = &((eSnapshot_Z80 *)s)->v2;
		word len = SwapWord(s2->len);
		ptr += 2 + len;
#else
		eSnapshot_Z80_v2 snap2;
		stream_read(stream, (uint8_t *)&snap2, sizeof(snap2), true);
		word len = SwapWord(snap2.len);
		s2 = &snap2;
		stream_skip(stream, 2 + len - sizeof(snap2));
#endif
		model48k = (s2->model < 3);
#ifdef NO_USE_128K
		if (!model48k) return false;
#endif
		SetupDevices(model48k);
		reg_pc = s2->newpc;
		byte* p48[] =
		{
			0, 0, 0, 0,
			memory->Get(eMemory::P_RAM2), memory->Get(eMemory::P_RAM0), 0, 0,
			memory->Get(eMemory::P_RAM5), 0, 0, 0
		};
#ifndef NO_USE_128K
		byte* p128[] =
		{
			0, 0, 0, memory->Get(eMemory::P_RAM0),
			memory->Get(eMemory::P_RAM1), memory->Get(eMemory::P_RAM2), memory->Get(eMemory::P_RAM3), memory->Get(eMemory::P_RAM4),
			memory->Get(eMemory::P_RAM5), memory->Get(eMemory::P_RAM6), memory->Get(eMemory::P_RAM7), 0
		};
#endif
		val7ffd = model48k ? 0x30 : s2->p7FFD;
#ifndef USE_STREAM
		while(ptr < (byte*)s + buf_size)
#else
		while((ptr = stream_peek(stream, 3)))
#endif
		{
			word len = ptr[0] | word(ptr[1]) << 8;
			if (ptr[2] > 11)
				return false;
#ifndef NO_USE_128K
			byte* dstpage = model48k ? p48[ptr[2]] : p128[ptr[2]];
#else
			byte *dstpage = p48[ptr[2]];
#endif
			if (!dstpage)
				return false;
//            printf("%d %04x\n", ptr[2], len);
#ifndef USE_STREAM
			ptr += 3;
#else
			stream_skip(stream, 3);
#endif
			if (len == 0xFFFF) {
#ifndef USE_STREAM
				memcpy(dstpage, ptr, len = eMemory::PAGE_SIZE);
#else
				stream_read(stream, dstpage, eMemory::PAGE_SIZE, true);
#endif
			} else {
#ifndef USE_STREAM
				UnpackPage(dstpage, eMemory::PAGE_SIZE, ptr, len);
				ptr += len;
#else
				UnpackPage(dstpage, eMemory::PAGE_SIZE, stream, len);
#endif
			}
		}
#ifndef NO_USE_AY
		devices->Get<eAY>()->SetRegs(s2->AY);
#endif
	}
	else
	{
		SetupDevices(true);
		int len = buf_size - 30;
		assert(model48k);
		len -= 4; // this seems to be the case! check assertions below
		if (flags&0x20) {
#ifndef USE_STREAM
			UnpackPage(memory->Get(eMemory::P_RAM5), 3*eMemory::PAGE_SIZE, ptr, len);
#else
			UnpackPage(memory->Get(eMemory::P_RAM5), 3*eMemory::PAGE_SIZE, stream, len);
#endif
		} else {
#ifndef USE_STREAM
			memcpy(memory->Get(eMemory::P_RAM5), ptr, MAX(3*eMemory::PAGE_SIZE, len));
#else
			stream_read(stream, memory->Get(eMemory::P_RAM5), MAX(3*eMemory::PAGE_SIZE, len), true);
#endif
		}
#ifndef NO_USE_128K
		static_assert(eMemory::P_RAM6 == eMemory::P_RAM5 + 1, "");
		static_assert(eMemory::P_RAM7 == eMemory::P_RAM6 + 1, "");
		memcpy(memory->Get(eMemory::P_RAM2), memory->Get(eMemory::P_RAM6), eMemory::PAGE_SIZE);
		memcpy(memory->Get(eMemory::P_RAM0), memory->Get(eMemory::P_RAM7), eMemory::PAGE_SIZE);
#endif
	}
#ifndef USE_Z80_ARM
	a = s->a, f = s->f;
	bc = SwapWord(s->bc), de = SwapWord(s->de), hl = SwapWord(s->hl);
	alt.bc = SwapWord(s->bc1), alt.de = SwapWord(s->de1), alt.hl = SwapWord(s->hl1);
	alt.a = s->a1, alt.f = s->f1;
	pc = SwapWord(reg_pc), sp = SwapWord(s->sp); ix = SwapWord(s->ix), iy = SwapWord(s->iy);

	i = s->i, r_low = s->r & 0x7F;
	r_hi = ((flags & 1) << 7);
	byte pFE = (flags >> 1) & 7;
	devices->IoWrite(0xfe, pFE, t);
	iff1 = s->iff1, iff2 = s->iff2; im = s->im & 3;
	devices->IoWrite(0x7ffd, val7ffd, t);
#else
	auto &rs = z80a_resting_state;
	rs.af = (s->a << 8) |  s->f;
	rs.bc = SwapWord(s->bc), rs.de = SwapWord(s->de), rs.hl = SwapWord(s->hl);
	rs.alt_bc = SwapWord(s->bc1), rs.alt_de = SwapWord(s->de1), rs.alt_hl = SwapWord(s->hl1);
	rs.alt_af = (s->a1 << 8) | s->f1;
	rs.pc = SwapWord(reg_pc), rs.sp = SwapWord(s->sp); rs.ix = SwapWord(s->ix), rs.iy = SwapWord(s->iy);

	rs.i = s->i, rs.r_low = s->r & 0x7F;
	rs.r_hi = ((flags & 1) << 7);
	byte pFE = (flags >> 1) & 7;
	devices->IoWrite(0xfe, pFE, rs.t);
	rs.iff1 = s->iff1, rs.iff2 = s->iff2; rs.im = s->im & 3;
	devices->IoWrite(0x7ffd, val7ffd, rs.t);
#endif
#ifndef NO_USE_128K
	if(model48k)
		devices->Get<eRom>()->SelectPage(eRom::ROM_48);
	else
	{
		assert(s2);
		devices->Get<eRom>()->SelectPage((s2->p7FFD & 0x10) ? eRom::ROM_128_0 : eRom::ROM_128_1);
	}
#else
	assert(model48k);
	devices->Get<eRom>()->SelectPage(eRom::ROM_48);
#endif
	return true;
}
#ifndef USE_STREAM
void eZ80Accessor::UnpackPage(byte* dst, int dstlen, const byte* src, int srclen)
{
	memset(dst, 0, dstlen);
	while(srclen > 0 && dstlen > 0)
	{
		if(srclen >= 4 && src[0] == 0xED && src[1] == 0xED)
		{
			for(byte i = src[2]; i; i--)
				*dst++ = src[3], dstlen--;
			srclen -= 4;
			src += 4;
		}
		else
		{
			*dst++ = *src++;
			--dstlen;
			--srclen;
		}
	}
	// added this to check compared to the streaming version; it is also off by 4 in some cases (currently handled in caller)
	assert(srclen == 0);
}

#else
void eZ80Accessor::UnpackPage(byte* dst, int dstlen, struct stream *stream, int srclen)
{
	memset(dst, 0, dstlen);
#ifndef NDEBUG
	uint32_t pos_target = stream_pos(stream) + srclen;
#endif
	while(dstlen > 0 && srclen > 0)
	{
		const byte *src = stream_peek(stream, 4);
		if (src && srclen >= 4 && src[0] == 0xED && src[1] == 0xED)
		{
			for(byte i = src[2]; i; i--)
				*dst++ = src[3], dstlen--;
			srclen -= 4;
			stream_skip(stream, 4);
		}
		else
		{
			if (!src) {
				src = stream_peek(stream, 1);
				if (!src) {
					assert(false);
					break;
				}
			}
			*dst++ = src[0];
			--dstlen;
			--srclen;
			stream_skip(stream, 1);
		}
	}
#ifndef NDEBUG
	uint pos = stream_pos(stream);
	// seems to be 4 off, the old code didn't care
	assert(pos == pos_target);
#endif
}
#endif

#ifndef NO_USE_SZX
bool LoadSZX(eSpeccy* speccy, const void* data, size_t data_size);
#endif

#ifndef USE_STREAM
bool Load(eSpeccy* speccy, const char* type, const void* data, size_t data_size)
{
	speccy->Devices().FrameStart(0);
	eZ80Accessor* z80 = (eZ80Accessor*)speccy->CPU();
	bool ok = false;
	if(!strcmp(type, "z80"))
		ok = z80->SetState((const eSnapshot_Z80*)data, data_size);
#ifndef NO_USE_SNA
	else if(!strcmp(type, "sna"))
		ok = z80->SetState((const eSnapshot_SNA*)data, data_size);
#endif
#ifndef NO_USE_SZX
	else if(!strcmp(type, "szx"))
		ok = LoadSZX(speccy, data, data_size);
#endif
	speccy->Devices().FrameUpdate();
	speccy->Devices().FrameEnd(z80->FrameTacts() + z80->T());
	return ok;
}
#else
bool Load(eSpeccy* speccy, const char* type, struct stream *stream)
{
		speccy->Devices().FrameStart(0);
		eZ80Accessor* z80 = (eZ80Accessor*)speccy->CPU();
		bool ok = false;
		if(!strcmp(type, "z80"))
			ok = z80->SetStateZ80(stream);
#ifndef NO_USE_SNA
		else if(!strcmp(type, "sna"))
			ok = z80->SetStateSNA(stream);
#endif
#ifndef NO_USE_SZX
		else if(!strcmp(type, "szx"))
		ok = LoadSZX(speccy, data, data_size);
#endif
		stream_close(stream);
		speccy->Devices().FrameUpdate();
		speccy->Devices().FrameEnd(z80->FrameTacts() + z80->T());
		return ok;
	}
#endif

#ifndef NO_USE_SAVE
bool Store(eSpeccy* speccy, const char* file)
{
	FILE* f = fopen(file, "wb");
	if(!f)
		return false;
	eSnapshot_SNA* s = new eSnapshot_SNA;
	eZ80Accessor* z80 = (eZ80Accessor*)speccy->CPU();
	size_t size = z80->StoreState(s);
	bool ok = false;
	if(size)
		ok = fwrite(s, 1, size, f) == size;
	delete s;
	fclose(f);
	return ok;
}
#endif

}
//namespace xSnapshot
