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

#include "../../std.h"
#include "../../speccy.h"
#include "../../z80/z80.h"
#include "../memory.h"
#include "tape.h"
#include <limits>
#include "../../options_common.h"

#ifndef NO_USE_TAPE

//=============================================================================
//	eTape::Init
//-----------------------------------------------------------------------------
void eTape::Init()
{
	eInherited::Init();
#ifdef USE_LEGACY_TAPE_COMPARISON
	max_pulses = 0;
	tape_err = 0;

	tape_image = NULL;
	tape_imagesize = 0;

	tapeinfo = NULL;
	tape_infosize = 0;

	appendable = 0;

#endif
	tape_instance = NULL;
	pulse_iterator = NULL;
}
//=============================================================================
//	eTape::Reset
//-----------------------------------------------------------------------------
void eTape::Reset()
{
	eInherited::Reset();
	ResetTape();
}
//=============================================================================
//	eTape::Start
//-----------------------------------------------------------------------------
void eTape::Start()
{
	StartTape();
}
//=============================================================================
//	eTape::Stop
//-----------------------------------------------------------------------------
void eTape::Stop()
{
	StopTape();
}
//=============================================================================
//	eTape::Started
//-----------------------------------------------------------------------------
bool eTape::Started() const
{
#ifdef USE_LEGACY_TAPE_COMPARISON
	assert((tape.play_pointer != NULL) == tape.playing);
#endif
	return tape.playing;
}
//=============================================================================
//	eTape::Inserted
//-----------------------------------------------------------------------------
bool eTape::Inserted() const
{
#ifdef USE_LEGACY_TAPE_COMPARISON
	assert((tape_image != NULL) == (tape_instance != NULL));
#endif
	return tape_instance != NULL;
}
#ifndef USE_HACKED_DEVICE_ABSTRACTION
//=============================================================================
//	eTape::IoRead
//-----------------------------------------------------------------------------
bool eTape::IoRead(word port) const
{
	return !(port&1);
}
#endif
//=============================================================================
//	eTape::IoRead
//-----------------------------------------------------------------------------
void eTape::IoRead(word port, byte* v, int tact)
{
	*v |= TapeBit(tact) & 0x40;
}
#ifdef USE_LEGACY_TAPE_COMPARISON
//=============================================================================
//	eTape::FindPulse
//-----------------------------------------------------------------------------
dword eTape::FindPulse(dword t)
{
	if(max_pulses < 0x100)
	{
		for(dword i = 0; i < max_pulses; i++)
			if(tape_pulse[i] == t)
				return i;
		tape_pulse[max_pulses] = t;
		return max_pulses++;
	}
	tape_err = 1;
	dword nearest = 0;
	int delta = 0x7FFFFFFF;
	for(dword i = 0; i < 0x100; i++)
		if(delta > abs((int)t - (int)tape_pulse[i]))
			nearest = i, delta = abs((int)t - (int)tape_pulse[i]);
	return nearest;
}
//=============================================================================
//	eTape::FindTapeIndex
//-----------------------------------------------------------------------------
void eTape::FindTapeIndex()
{
	for(dword i = 0; i < tape_infosize; i++)
		if(tape.play_pointer >= tape_image + tapeinfo[i].pos)
			tape.index = i;
}
#ifndef USE_MU_SIMPLIFICATIONS
//=============================================================================
//	eTape::FindTapeSizes
//-----------------------------------------------------------------------------
void eTape::FindTapeSizes()
{
	for(dword i = 0; i < tape_infosize; i++)
	{
		dword end = (i == tape_infosize - 1) ? tape_imagesize
				: tapeinfo[i + 1].pos;
		dword sz = 0;
		for(dword j = tapeinfo[i].pos; j < end; j++)
			sz += tape_pulse[tape_image[j]];
		tapeinfo[i].t_size = sz;
	}
}
#endif
#endif
//=============================================================================
//	eTape::StopTape
//-----------------------------------------------------------------------------
void eTape::StopTape()
{
#ifdef USE_LEGACY_TAPE_COMPARISON
	FindTapeIndex();
	if(tape.play_pointer >= tape.end_of_tape)
		tape.index = 0;
	tape.play_pointer = 0;
#endif
//    tape.edge_change = 0x7FFFFFFFFFFFFFFFLL;
	tape.edge_change = std::numeric_limits<qword>::max();
	tape.playing = false;
	tape.tape_bit =  0xff;
#ifndef NO_USE_FAST_TAPE
	speccy->CPU()->HandlerStep(NULL);
#endif
}
//=============================================================================
//	eTape::ResetTape
//-----------------------------------------------------------------------------
void eTape::ResetTape()
{
	pulse_iterator = tape_instance ? tape_instance->reset() : NULL;
#ifdef USE_LEGACY_TAPE_COMPARISON
	tape.index = 0;
	tape.play_pointer = 0;
#endif
	tape.tape_bit = 0xff;
	tape.edge_change = 0x7FFFFFFFFFFFFFFFLL;
#ifndef NO_USE_FAST_TAPE
	speccy->CPU()->HandlerStep(NULL);
#endif
	if (tape_instance) {
		pulse_iterator = tape_instance->reset();
	} else {
		pulse_iterator = NULL;
	}
	tape.playing = false;
}
//=============================================================================
//	eTape::StartTape
//-----------------------------------------------------------------------------
void eTape::StartTape()
{
	if(!Inserted())
		return;
#ifdef USE_LEGACY_TAPE_COMPARISON
	tape.play_pointer = tape_image + tapeinfo[tape.index].pos;
	tape.end_of_tape = tape_image + tape_imagesize;
#endif
	tape.edge_change = speccy->T();
	tape.tape_bit = 0xff;
	tape.playing = true;
//	speccy->CPU()->FastEmul(FastTapeEmul);
}
//=============================================================================
//	eTape::CloseTape
//-----------------------------------------------------------------------------
void eTape::CloseTape()
{
#ifdef USE_LEGACY_TAPE_COMPARISON
	if(tape_image)
	{
		mu_free(tape_image);
		tape_image = 0;
	}
	if(tapeinfo)
	{
		mu_free(tapeinfo);
		tapeinfo = 0;
	}
	tape.play_pointer = 0; // stop tape
	tape.index = 0; // rewind tape
	tape_err = max_pulses = tape_imagesize = tape_infosize = 0;
#endif
	tape.edge_change = 0x7FFFFFFFFFFFFFFFLL;
	tape.tape_bit = 0xff;
	pulse_iterator = NULL;
	delete tape_instance;
	tape_instance = NULL;
}

#ifdef USE_LEGACY_TAPE_COMPARISON
#define align_by(a,b) (((dword)(a) + ((b)-1)) & ~((b)-1))

//=============================================================================
//	eTape::Reserve
//-----------------------------------------------------------------------------
void eTape::Reserve(dword datasize)
{
	const int blocksize = 16384;
	dword newsize = align_by(datasize+tape_imagesize+1, blocksize);
	if(!tape_image)
		tape_image = (byte*)malloc(newsize);
	if(align_by(tape_imagesize, blocksize) < newsize)
		tape_image = (byte*)realloc(tape_image, newsize);
}
//=============================================================================
//	eTape::MakeBlock
//-----------------------------------------------------------------------------
void eTape::MakeBlock(const byte* data, dword size, dword pilot_t, dword s1_t,
		dword s2_t, dword zero_t, dword one_t, dword pilot_len, dword pause,
		byte last)
{
	Reserve(size * 16 + pilot_len + 3);
	if(pilot_len != (dword)-1)
	{
		dword t = FindPulse(pilot_t);
		for(dword i = 0; i < pilot_len; i++)
			tape_image[tape_imagesize++] = t;
		tape_image[tape_imagesize++] = FindPulse(s1_t);
		tape_image[tape_imagesize++] = FindPulse(s2_t);
	}
	dword t0 = FindPulse(zero_t), t1 = FindPulse(one_t);
	for(; size > 1; size--, data++)
		for(byte j = 0x80; j; j >>= 1)
			tape_image[tape_imagesize++] = (*data & j) ? t1 : t0, tape_image[tape_imagesize++]
					= (*data & j) ? t1 : t0;
	for(byte j = 0x80; j != (byte)(0x80 >> last); j >>= 1) // last byte
		tape_image[tape_imagesize++] = (*data & j) ? t1 : t0, tape_image[tape_imagesize++]
				= (*data & j) ? t1 : t0;
	if(pause)
		tape_image[tape_imagesize++] = FindPulse(pause * 3500);
}
//=============================================================================
//	eTape::Desc
//-----------------------------------------------------------------------------
void eTape::Desc(const byte* data, dword size, char* dst)
{
#ifdef USE_MU_SIMPLIFICATIONS
	dst[0] = 0;
#else
	byte crc = 0;
	char prg[10];
	dword i; //Alone Coder 0.36.7
	for(/*dword*/i = 0; i < size; i++)
		crc ^= data[i];
	if(!*data && size == 19 && (data[1] == 0 || data[1] == 3))
	{
		for(i = 0; i < 10; i++)
			prg[i] = (data[i + 2] < ' ' || data[i + 2] >= 0x80) ? '?' : data[i
					+ 2];
		for(i = 9; i && prg[i] == ' '; prg[i--] = 0)
			;
		sprintf(dst, "%s: \"%s\" %d,%d", data[1] ? "Bytes" : "Program", prg,
				Word(data + 14), Word(data + 12));
	}
	else if(*data == 0xFF)
		sprintf(dst, "data block, %d bytes", size - 2);
	else
		sprintf(dst, "#%02X block, %d bytes", *data, size - 2);
	sprintf(dst + strlen(dst), ", crc %s", crc ? "bad" : "ok");
#endif
}
//=============================================================================
//	eTape::AllocInfocell
//-----------------------------------------------------------------------------
void eTape::AllocInfocell()
{
	tapeinfo = (TAPEINFO*)realloc(tapeinfo, (tape_infosize + 1)
			* sizeof(TAPEINFO));
	tapeinfo[tape_infosize].pos = tape_imagesize;
	appendable = 0;
}
#endif
//=============================================================================
//	eTape::Open
//-----------------------------------------------------------------------------
#ifdef USE_STREAM
bool eTape::Open(const char* type, struct stream *stream)
{
	if(!strcmp(type, "tap"))
		return OpenTAP(stream);
#ifndef NO_USE_CSW
	else if(!strcmp(type, "csw"))
		return ParseCSW(stream);
#endif
#ifndef NO_USE_TZX
	else if(!strcmp(type, "tzx"))
		return ParseTZX(stream);
#endif
	return false;
}
#endif
//=============================================================================
//	eTape::ParseTAP
//-----------------------------------------------------------------------------
bool eTape::OpenTAP(struct stream *stream)
{
	CloseTape();
	tape_instance = new eTapeInstanceTAP(stream);

#ifndef NDEBUG
	do {
		if (!stream_peek(stream, 1)) break; // nothing left
		const uint8_t *size_ptr = stream_peek(stream, 2);
		if (!size_ptr) {
			return false;
		}
		int size = Word(size_ptr);
		if (!size) break;
		if (!stream_skip(stream, size + 2)) {
			return false;
		}
	} while (true);
	stream_reset(stream);
#endif
	return true;
}

#ifndef NO_USE_CSW
//=============================================================================
//	eTape::ParseCSW
//-----------------------------------------------------------------------------
bool eTape::ParseCSW(const void* data, size_t data_size)
{
	const byte* buf = (const byte*)data;
	const dword Z80FQ = 3500000;
	CloseTape();
	NamedCell("CSW tape image");
	if(buf[0x1B] != 1)
		return false; // unknown compression type
	dword rate = Z80FQ / Word(buf + 0x19); // usually 3.5mhz / 44khz
	if(!rate)
		return false;
	Reserve(data_size - 0x18);
	if(!(buf[0x1C] & 1))
		tape_image[tape_imagesize++] = FindPulse(1);
	for(const byte* ptr = (const byte*)data + 0x20; ptr < (const byte*)data + data_size;)
	{
		dword len = *ptr++ * rate;
		if(!len)
		{
			len = Dword(ptr) / rate;
			ptr += 4;
		}
		tape_image[tape_imagesize++] = FindPulse(len);
	}
	tape_image[tape_imagesize++] = FindPulse(Z80FQ / 10);
	FindTapeSizes();
	return true;
}
#endif
#ifndef NO_USE_TZX
//=============================================================================
//	eTape::CreateAppendableBlock
//-----------------------------------------------------------------------------
void eTape::CreateAppendableBlock()
{
	if(!tape_infosize || appendable)
		return;
	NamedCell("set of pulses");
	appendable = 1;
}
//=============================================================================
//	eTape::ParseTZX
//-----------------------------------------------------------------------------
bool eTape::ParseTZX(const void* data, size_t data_size)
{
	byte* ptr = (byte*)data;
	CloseTape();
	dword size, pause, i, j, n, t, t0;
	byte pl, last, *end;
	byte* p;
	dword loop_n = 0, loop_p = 0;
	char nm[512];
	while(ptr < (const byte*)data + data_size)
	{
		switch(*ptr++)
		{
			case 0x10: // normal block
				AllocInfocell();
				size = Word(ptr + 2);
				pause = Word(ptr);
				ptr += 4;
				tape_infosize++;
				MakeBlock(ptr, size, 2168, 667, 735, 855, 1710, (*ptr < 4) ? 8064
																		   : 3220, pause);
				ptr += size;
				break;
			case 0x11: // turbo block
				AllocInfocell();
				size = 0xFFFFFF & Dword(ptr + 0x0F);
				tape_infosize++;
				MakeBlock(ptr + 0x12, size, Word(ptr), Word(ptr + 2),
						  Word(ptr + 4), Word(ptr + 6), Word(ptr + 8),
						  Word(ptr + 10), Word(ptr + 13), ptr[12]);
				// todo: test used bits - ptr+12
				ptr += size + 0x12;
				break;
			case 0x12: // pure tone
				CreateAppendableBlock();
				pl = FindPulse(Word(ptr));
				n = Word(ptr + 2);
				Reserve(n);
				for(i = 0; i < n; i++)
					tape_image[tape_imagesize++] = pl;
				ptr += 4;
				break;
			case 0x13: // sequence of pulses of different lengths
				CreateAppendableBlock();
				n = *ptr++;
				Reserve(n);
				for(i = 0; i < n; i++, ptr += 2)
					tape_image[tape_imagesize++] = FindPulse(Word(ptr));
				break;
			case 0x14: // pure data block
				CreateAppendableBlock();
				size = 0xFFFFFF & Dword(ptr + 7);
				MakeBlock(ptr + 0x0A, size, 0, 0, 0, Word(ptr),
						  Word(ptr + 2), -1, Word(ptr + 5), ptr[4]);
				ptr += size + 0x0A;
				break;
			case 0x15: // direct recording
				size = 0xFFFFFF & Dword(ptr + 5);
				t0 = Word(ptr);
				pause = Word(ptr + 2);
				last = ptr[4];
				NamedCell("direct recording");
				ptr += 8;
				pl = 0;
				n = 0;
				for(i = 0; i < size; i++) // count number of pulses
					for(j = 0x80; j; j >>= 1)
						if((ptr[i] ^ pl) & j)
							n++, pl ^= -1;
				t = 0;
				pl = 0;
				Reserve(n + 2);
				for(i = 1; i < size; i++, ptr++) // find pulses
					for(j = 0x80; j; j >>= 1)
					{
						t += t0;
						if((*ptr ^ pl) & j)
						{
							tape_image[tape_imagesize++] = FindPulse(t);
							pl ^= -1;
							t = 0;
						}
					}
				// find pulses - last byte
				for(j = 0x80; j != (byte)(0x80 >> last); j >>= 1)
				{
					t += t0;
					if((*ptr ^ pl) & j)
					{
						tape_image[tape_imagesize++] = FindPulse(t);
						pl ^= -1;
						t = 0;
					}
				}
				ptr++;
				tape_image[tape_imagesize++] = FindPulse(t); // last pulse ???
				if(pause)
					tape_image[tape_imagesize++] = FindPulse(pause * 3500);
				break;
			case 0x20: // pause (silence) or 'stop the tape' command
				pause = Word(ptr);
				sprintf(nm, pause ? "pause %d ms" : "stop the tape", pause);
				NamedCell(nm);
				Reserve(2);
				ptr += 2;
				if(!pause)
				{ // at least 1ms pulse as specified in TZX 1.13
					tape_image[tape_imagesize++] = FindPulse(3500);
					pause = -1;
				}
				else
					pause *= 3500;
				tape_image[tape_imagesize++] = FindPulse(pause);
				break;
			case 0x21: // group start
				n = *ptr++;
				NamedCell(ptr, n);
				ptr += n;
				appendable = 1;
				break;
			case 0x22: // group end
				break;
			case 0x23: // jump to block
				NamedCell("* jump");
				ptr += 2;
				break;
			case 0x24: // loop start
				loop_n = Word(ptr);
				loop_p = tape_imagesize;
				ptr += 2;
				break;
			case 0x25: // loop end
				if(!loop_n)
					break;
				size = tape_imagesize - loop_p;
				Reserve((loop_n - 1) * size);
				for(i = 1; i < loop_n; i++)
					memcpy(tape_image + loop_p + i * size, tape_image + loop_p,
						   size);
				tape_imagesize += (loop_n - 1) * size;
				loop_n = 0;
				break;
			case 0x26: // call
				NamedCell("* call");
				ptr += 2 + 2 * Word(ptr);
				break;
			case 0x27: // ret
				NamedCell("* return");
				break;
			case 0x28: // select block
				sprintf(nm, "* choice: ");
				n = ptr[2];
				p = ptr + 3;
				for(i = 0; i < n; i++)
				{
					if(i)
						strcat(nm, " / ");
					char *q = nm + strlen(nm);
					size = *(p + 2);
					memcpy(q, p + 3, size);
					q[size] = 0;
					p += size + 3;
				}
				NamedCell(nm);
				ptr += 2 + Word(ptr);
				break;
			case 0x2A: // stop if 48k
				NamedCell("* stop if 48K");
				ptr += 4 + Dword(ptr);
				break;
			case 0x30: // text description
				n = *ptr++;
				NamedCell(ptr, n);
				ptr += n;
				appendable = 1;
				break;
			case 0x31: // message block
				NamedCell("- MESSAGE BLOCK ");
				end = ptr + 2 + ptr[1];
				pl = *end;
				*end = 0;
				for(p = ptr + 2; p < end; p++)
					if(*p == 0x0D)
						*p = 0;
				for(p = ptr + 2; p < end; p += strlen((char*)p) + 1)
					NamedCell(p);
				*end = pl;
				ptr = end;
				NamedCell("-");
				break;
			case 0x32: // archive info
				NamedCell("- ARCHIVE INFO ");
				p = ptr + 3;
				for(i = 0; i < ptr[2]; i++)
				{
					const char *info;
					switch(*p++)
					{
						case 0:
							info = "Title";
							break;
						case 1:
							info = "Publisher";
							break;
						case 2:
							info = "Author";
							break;
						case 3:
							info = "Year";
							break;
						case 4:
							info = "Language";
							break;
						case 5:
							info = "Type";
							break;
						case 6:
							info = "Price";
							break;
						case 7:
							info = "Protection";
							break;
						case 8:
							info = "Origin";
							break;
						case 0xFF:
							info = "Comment";
							break;
						default:
							info = "info";
							break;
					}
					dword size = *p + 1;
					char tmp = p[size];
					p[size] = 0;
					sprintf(nm, "%s: %s", info, p + 1);
					p[size] = tmp;
					p += size;
					NamedCell(nm);
				}
				NamedCell("-");
				ptr += 2 + Word(ptr);
				break;
			case 0x33: // hardware type
				ptr += 1 + 3 * *ptr;
				break;
			case 0x34: // emulation info
				NamedCell("* emulation info");
				ptr += 8;
				break;
			case 0x35: // custom info
				if(!memcmp(ptr, "POKEs           ", 16))
				{
					NamedCell("- POKEs block ");
					NamedCell(ptr + 0x15, ptr[0x14]);
					p = ptr + 0x15 + ptr[0x14];
					n = *p++;
					for(i = 0; i < n; i++)
					{
						NamedCell(p + 1, *p);
						p += *p + 1;
						t = *p++;
						strcpy(nm, "POKE ");
						for(j = 0; j < t; j++)
						{
							sprintf(nm + strlen(nm), "%d,", Word(p + 1));
							sprintf(nm + strlen(nm), *p & 0x10 ? "nn" : "%d",
									*(byte*)(p + 3));
							if(!(*p & 0x08))
								sprintf(nm + strlen(nm), "(page %d)", *p & 7);
							strcat(nm, "; ");
							p += 5;
						}
						NamedCell(nm);
					}
					nm[0] = '-';
					nm[1] = 0;
					nm[2] = 0;
					nm[3] = 0;
				}
				else
					sprintf(nm, "* custom info: %s", ptr), nm[15 + 16] = 0;
				NamedCell(nm);
				ptr += 0x14 + Dword(ptr + 0x10);
				break;
			case 0x40: // snapshot
				NamedCell("* snapshot");
				ptr += 4 + (0xFFFFFF & Dword(ptr + 1));
				break;
			case 0x5A: // 'Z'
				ptr += 9;
				break;
			default:
				ptr += data_size;
		}
	}

	if(tape_imagesize && tape_pulse[tape_image[tape_imagesize - 1]] < 350000)
		Reserve(1), tape_image[tape_imagesize++] = FindPulse(350000); // small pause [rqd for 3ddeathchase]
	FindTapeSizes();
	return (ptr == (const byte*)data + data_size);
}
#endif
//=============================================================================
//	eTape::TapeBit
//-----------------------------------------------------------------------------
byte eTape::TapeBit(int tact)
{
	qword cur = speccy->T() + tact;
	while(cur > tape.edge_change)
	{
		dword t = (dword)(tape.edge_change - speccy->T());
		if((int)t >= 0)
		{
//			const short vol = 4096;//1000; // a little loader than the default
//			short mono = tape.tape_bit ? vol : 0;
//            mono = (mono * 182 * xPlatform::OpTapeVolume()) >> 16; //(0->255)
			// max out at 160/256
			uint8_t mono = tape.tape_bit ? (xPlatform::OpTapeVolume() << 4) : 0;
			Update(tact, mono, mono);
		}
		dword pulse;
		tape.tape_bit ^= 0xff;
		if ((pulse = NextPulseOrStop()) != (dword)-1) {
			tape.edge_change += pulse;
		}
	}
	return (byte)tape.tape_bit;
}

dword eTape::NextPulseOrStop()
{
	dword pulse;
	assert(pulse_iterator && tape.playing);
	pulse = (pulse_iterator && tape.playing) ? pulse_iterator->next_pulse_length() : -1;
#ifdef USE_LEGACY_TAPE_COMPARISON
	dword pulse2;
	if(tape.play_pointer >= tape.end_of_tape ||
	   (pulse2 = tape_pulse[*tape.play_pointer++]) == (dword)-1) {
		assert(pulse == -1);
	}
	else
	{
		assert(pulse == pulse2);
	}
#endif
	if (pulse == (dword)-1)
	{
		StopTape();
	}
	return pulse;
}

void eTape::SkipPulse() {
	assert(pulse_iterator);
	pulse_iterator->next_pulse_length();
#ifdef USE_LEGACY_TAPE_COMPARISON
	tape.play_pointer++;
#endif
}

#ifndef NO_USE_FAST_TAPE
namespace xZ80
{

//*****************************************************************************
//	eZ80_FastTape
//-----------------------------------------------------------------------------
	class eZ80_FastTape: public xZ80::eZ80
	{
	public:
		void StepEdge();
		void StepTrap();
		void Step()
		{
//	    static int foo = 0;
//	    printf("Tape step %d %04x\n", foo++, get_caller_pc());
			StepTrap();
			StepEdge();
		}
		void FindEdge(byte mask, int tstates) {
			FindEdge(mask, tstates, 0xff, 1);
		}
		void FindEdge(byte mask, int tstates, byte endb, int delta_b);
		bool CompareMemory(word addr, byte *cmp, int len)
		{
			while (len--)
			{
				if (*(cmp++) != memory->Read(addr++))
					return false;
			}
			return true;
		}
	};

//=============================================================================
//	eZ80_FastTape::StepEdge
//-----------------------------------------------------------------------------
	void eZ80_FastTape::StepEdge()
	{
		const word pc = get_caller_pc();
		byte p0 = memory->Read(pc + 0);
		byte p1 = memory->Read(pc + 1);
		byte p2 = memory->Read(pc + 2);
		byte p3 = memory->Read(pc + 3);
		if(p0 == 0x3D && p1 == 0x20 && p2 == 0xFD && p3 == 0xA7)
		{ // dec a:jr nz,$-1
			delta_caller_t(((byte)(get_caller_a() - 1)) * 16);
			set_caller_a(1);
			return;
		}
		if(p0 == 0x10 && p1 == 0xFE)
		{ // djnz $
			delta_caller_t(((byte)(get_caller_b() - 1)) * 13);
			set_caller_b(1);
			return;
		}
		if(p0 == 0x3D && p1 == 0xC2 && pc == dword(p3) * 0x100 + p2)
		{ // dec a:jp nz,$-1
			delta_caller_t(((byte)(get_caller_a() - 1)) * 14);
			set_caller_a(1);
			return;
		}
		if(p0 == 0x04 && p1 == 0xC8 && p2 == 0x3E)
		{
			byte p04 = memory->Read(pc + 4);
			byte p05 = memory->Read(pc + 5);
			byte p06 = memory->Read(pc + 6);
			byte p07 = memory->Read(pc + 7);
			byte p08 = memory->Read(pc + 8);
			byte p09 = memory->Read(pc + 9);
			byte p10 = memory->Read(pc + 10);
			byte p11 = memory->Read(pc + 11);
			byte p12 = memory->Read(pc + 12);
			if(p04 == 0xDB && p05 == 0xFE && p06 == 0x1F && p07 == 0xD0 && p08
																		   == 0xA9 && p09 == 0xE6 && p10 == 0x20 && p11 == 0x28 && p12
																																   == 0xF3)
			{ // find edge (rom routine)
				FindEdge(0x20, 59);
				return;
			}
			if(p04 == 0xDB && p05 == 0xFE && p06 == 0xCB && p07 == 0x1F && p08
																		   == 0xA9 && p09 == 0xE6 && p10 == 0x20 && p11 == 0x28 && p12
																																   == 0xF3)
			{ // rra,ret nc => rr a (popeye2)
				FindEdge(0x20, 58);
				return;
			}
			if(p04 == 0xDB && p05 == 0xFE && p06 == 0x1F && p07 == 0x00 && p08
																		   == 0xA9 && p09 == 0xE6 && p10 == 0x20 && p11 == 0x28 && p12
																																   == 0xF3)
			{ // ret nc nopped (some bleep loaders)
				FindEdge(0x20, 58);
				return;
			}
			if(p04 == 0xDB && p05 == 0xFE && p06 == 0xA9 && p07 == 0xE6 && p08
																		   == 0x40 && p09 == 0xD8 && p10 == 0x00 && p11 == 0x28 && p12
																																   == 0xF3)
			{ // no rra, no break check (rana rama)
				FindEdge(0x40, 59);
				return;
			}
			if(p04 == 0xDB && p05 == 0xFE && p06 == 0x1F && p07 == 0xA9 && p08
																		   == 0xE6 && p09 == 0x20 && p10 == 0x28 && p11 == 0xF4)
			{ // ret nc skipped: routine without BREAK checking (ZeroMusic & JSW)
				FindEdge(0x20, 54);
				return;
			}
		}
		if(p0 == 0x04 && p1 == 0x20 && p2 == 0x03)
		{
			byte p06 = memory->Read(pc + 6);
			byte p08 = memory->Read(pc + 8);
			byte p09 = memory->Read(pc + 9);
			byte p10 = memory->Read(pc + 10);
			byte p11 = memory->Read(pc + 11);
			byte p12 = memory->Read(pc + 12);
			byte p13 = memory->Read(pc + 13);
			byte p14 = memory->Read(pc + 14);
			if(p06 == 0xDB && p08 == 0x1F && p09 == 0xC8 && p10 == 0xA9 && p11
																		   == 0xE6 && p12 == 0x20 && p13 == 0x28 && p14 == 0xF1)
			{ // find edge from Donkey Kong
				FindEdge(0x20, 59);
				return;
			}
		}
		if(p0 == 0x3E && p2 == 0xDB && p3 == 0xFE)
		{
			byte p04 = memory->Read(pc + 4);
			byte p05 = memory->Read(pc + 5);
			byte p06 = memory->Read(pc + 6);
			byte p07 = memory->Read(pc + 7);
			byte p09 = memory->Read(pc + 9);
			byte p10 = memory->Read(pc + 10);
			byte p11 = memory->Read(pc + 11);
			if(p04 == 0xA9 && p05 == 0xE6 && p06 == 0x40 && p07 == 0x20 && p09
																		   == 0x05 && p10 == 0x20 && p11 == 0xF4)
			{ // lode runner
				FindEdge(0x40, 52, 1, -1);
			}
		}
	}

	void eZ80_FastTape::FindEdge(byte mask, int tstates, byte end_b, int delta_b)
	{
		eTape *tape = devices->Get<eTape>();
		for (;;)
		{
			if (get_caller_b() == end_b)
				return;
			if ((tape->TapeBit(T()) ^ get_caller_c()) & mask)
				return;
			set_caller_b(get_caller_b() + delta_b);
			delta_caller_t(tstates);
		}
	}

//=============================================================================
//	eZ80_FastTape::StepTrap
//-----------------------------------------------------------------------------
	void eZ80_FastTape::StepTrap()
	{
#if 1
		const word pc = get_caller_pc();
		if ((pc & 0xFFFF) != 0x056B) return;
		eTape* tape = devices->Get<eTape>();
		dword pulse;
		do
		{
			if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
				return;
			}
		}
		while(pulse > 770);
		tape->SkipPulse();

		// loading header
		byte l = 0;
		for(dword bit = 0x80; bit; bit >>= 1)
		{
			if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
				set_caller_l(l);
				set_caller_pc(0x05E2);
				return;
			}
			l |= (pulse > 1240) ? bit : 0;
			tape->SkipPulse();
		}

		// loading data
		word de = get_caller_de();
		do
		{
			l = 0;
			for(dword bit = 0x80; bit; bit >>= 1)
			{
				if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
					set_caller_l(l);
					set_caller_pc(0x05E2);
					return;
				}
				l |= (pulse > 1240) ? bit : 0;
				tape->SkipPulse();
			}
			word ix = get_caller_ix();
			memory->Write(ix++, l);
			set_caller_ix(ix);
			--de;
			set_caller_de(de);
		}
		while(de & 0xFFFF);

		// loading CRC
		l = 0;
		for(dword bit = 0x80; bit; bit >>= 1)
		{
			if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
				set_caller_l(l);
				set_caller_pc(0x05E2);
				return;
			}
			l |= (pulse > 1240) ? bit : 0;
			tape->SkipPulse();
		}
		set_caller_pc(0x05DF);
		set_caller_flag(CF);
		set_caller_bc(0xB001);
		set_caller_h(0);
		set_caller_l(l);
#endif
#if 0
		const word pc = get_caller_pc();
	if((pc & 0xFFFF) != 0x056B)
		return;
	eTape* tape = devices->Get<eTape>();
	dword pulse;
	do
	{
		if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
			return;
		}
	}
	while(pulse > 770);
	tape->SkipPulse();

	// loading header
	set_caller_l(0);
	for(dword bit = 0x80; bit; bit >>= 1)
	{
		if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
			set_caller_pc(0x05E2);
			return;
		}
		set_caller_l(get_caller_l() | (pulse > 1240) ? bit : 0);
		tape->SkipPulse();
	}

	// loading data
	do
	{
		set_caller_l(0);
		for(dword bit = 0x80; bit; bit >>= 1)
		{
			if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
				set_caller_pc(0x05E2);
				return;
			}
			set_caller_l(get_caller_l() | (pulse > 1240) ? bit : 0);
			tape->SkipPulse();
		}
		word ix = get_caller_ix();
		memory->Write(ix, get_caller_l());
		set_caller_ix(ix + 1);
		set_caller_de(get_caller_de()-1);
	}
	while(get_caller_de() & 0xFFFF);

	// loading CRC
	set_caller_l(0);
	for(dword bit = 0x80; bit; bit >>= 1)
	{
		if ((pulse = tape->NextPulseOrStop()) == (dword)-1) {
			set_caller_pc(0x05E2);
			return;
		}
		set_caller_l(get_caller_l() | (pulse > 1240) ? bit : 0);
		tape->SkipPulse();
	}
	set_caller_pc(0x05DF);
	set_caller_f(get_caller_f() | CF);
	set_caller_bc(0xB001);
	set_caller_h(0);

#endif
	}


}
//namespace xZ80

static class eFastTapeEmul : public xZ80::eZ80::eHandlerStep
{
	virtual void Z80_Step(xZ80::eZ80* z80)
	{
		((xZ80::eZ80_FastTape*)z80)->Step();
	}
} fte;

xZ80::eZ80::eHandlerStep* fast_tape_emul = &fte;
#endif
#endif