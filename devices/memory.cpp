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
#include "../platform/io.h"
#include "memory.h"

#ifdef USE_EMBEDDED_RESOURCES
#ifndef NO_USE_128K
#include "res/rom/sos128_0.h"
#include "res/rom/sos128_1.h"
#include "res/rom/service.h"
#endif
#ifndef NO_USE_DOS
#include "res/rom/dos513f.h"
#endif
#include "res/rom/sos48.h"
#endif//USE_EMBEDDED_RESOURCES

#ifdef USE_EXTERN_RESOURCES
extern byte sos128_0[];
extern byte sos128_1[];
extern byte sos48[];
extern byte service[];
extern byte dos513f[];
#endif//USE_EXTERN_RESOURCES

#ifdef NO_FLASH
#define USE_COMPRESSED_ROM
#endif

#ifdef USE_COMPRESSED_ROM
#include "stream.h"
#include "roms/roms.h"
#endif

#ifdef USE_SINGLE_64K_MEMORY
#ifndef USE_COMPRESSED_ROM
#include "ram64k.h"
#endif

#ifndef NO_USE_128K
#error cant handle 128K ROM with single 64K memory buffer
#endif
#endif

//=============================================================================
//	eMemory::eMemory
//-----------------------------------------------------------------------------
eMemory::eMemory() : memory(NULL)
{
#ifndef USE_SINGLE_64K_MEMORY
	memory = new byte[SIZE];
	memset(memory, 0, SIZE);
#else
#ifdef USE_COMPRESSED_ROM
	memory =(uint8_t *) calloc(1, 65536);
#else
	memory = ram64k;
#endif
	memset(memory + PAGE_SIZE, 0, 3 * PAGE_SIZE);
#endif
}
//=============================================================================
//	eMemory::~eMemory
//-----------------
// ------------------------------------------------------------
#ifndef NO_USE_DESTRUCTORS
eMemory::~eMemory()
{
#ifndef USE_SINGLE_64K_MEMORY
	SAFE_DELETE_ARRAY(memory);
#endif
}
#endif
//=============================================================================
//	eMemory::SetPage
//-----------------------------------------------------------------------------
void eMemory::SetPage(int idx, int page)
{
#ifdef USE_BANKED_MEMORY_ACCESS
	byte* addr = Get(page);
	addr -= 0x4000 * idx;
	bank_read[idx] = addr;
	if (idx)
	{
		bank_write[idx] = addr;
	}
#else
	assert(false);
#endif
}
//=============================================================================
//	eMemory::Page
//-----------------------------------------------------------------------------
int	eMemory::Page(int idx)
{
#ifdef USE_BANKED_MEMORY_ACCESS
	byte* addr = bank_read[idx];
	for(int p = 0; p < P_AMOUNT; ++p)
	{
		if(Get(p) == addr)
			return p;
	}
	assert(false);
	return -1;
#else
	assert(false);
	return 0;
#endif
}

//=============================================================================
//	eRom::LoadRom
//-----------------------------------------------------------------------------
void eRom::LoadRom(int page, const char* rom)
{
#ifdef USE_SINGLE_64K_MEMORY
	panic("Warning should not be loading ROM for 64K\n");
#endif
#if !defined(USE_COMPRESSED_ROM) && !defined(USE_EMBEDDED_RESOURCES) && !defined(USE_EXTERN_RESOURCES) && !defined(USE_EMBEDDED_FILES)
	FILE* f = fopen(rom, "rb");
	assert(f);
	size_t s = fread(memory->Get(page), 1, eMemory::PAGE_SIZE, f);
	assert(s == eMemory::PAGE_SIZE);
	fclose(f);
#else
	assert(false);
#endif
}

#ifdef USE_COMPRESSED_ROM
static void decompress_rom_to_memory(uint8_t *dest, const char *name)
{
	for(int i=0;i<embedded_rom_count;i++) {
		if (!strcmp(name, embedded_roms[i].name)) {
			struct stream *memory_stream = xip_stream_open(embedded_roms[i].data_z, embedded_roms[i].data_z_size, 1024, 10);
			struct stream *compressed_stream = compressed_stream_open(memory_stream, embedded_roms[i].data_size);
			stream_read(compressed_stream, dest, embedded_roms[i].data_size, true);
			stream_close(compressed_stream); // closes memory stream too.
			return;
		}
	}
	assert(false);
}
#endif
//=============================================================================
//	eRom::Init
//-----------------------------------------------------------------------------
void eRom::Init()
{
#ifdef USE_COMPRESSED_ROM
#ifndef USE_OVERLAPPED_ROMS
	// todo fix the inherited ugly overloading of enums
	init_rom_contents((eMemory::ePage)ROM_48, ROM_48);
#ifndef NO_USE_128K
#if 0
	decompress_rom_to_memory(memory->Get(ROM_128_0), rom128_0_z, rom128_0_z_len, rom128_0_len);
	decompress_rom_to_memory(memory->Get(ROM_128_1), rom128_1_z, rom128_1_z_len, rom128_1_len);
#else
	init_rom_contents((eMemory::ePage)ROM_128_0, ROM_128_0);
	init_rom_contents((eMemory::ePage)ROM_128_1, ROM_128_1);
#endif
#endif
#endif
#else
#if defined(USE_EMBEDDED_RESOURCES) || defined(USE_EXTERN_RESOURCES)
#ifndef NO_USE_128K
	memcpy(memory->Get(ROM_128_0),	sos128_0,	eMemory::PAGE_SIZE);
	memcpy(memory->Get(ROM_128_1),	sos128_1,	eMemory::PAGE_SIZE);
#endif
	memcpy(memory->Get(ROM_48),		sos48,		eMemory::PAGE_SIZE);
#ifndef NO_USE_128K
	memcpy(memory->Get(ROM_SYS),	service,	eMemory::PAGE_SIZE);
#endif
#ifndef NO_USE_DOS
	memcpy(memory->Get(ROM_DOS),	dos513f,	eMemory::PAGE_SIZE);
#endif
#else//USE_EMBEDDED_RESOURCES
#ifndef NO_USE_128K
	LoadRom(ROM_128_0,	xIo::ResourcePath("res/rom/sos128_0.rom"));
	LoadRom(ROM_128_1,	xIo::ResourcePath("res/rom/sos128_1.rom"));
#endif
	LoadRom(ROM_48,		xIo::ResourcePath("res/rom/sos48.rom"));
#ifndef NO_USE_128K
#ifndef NO_USE_SERVICE_ROM
	LoadRom(ROM_SYS,	xIo::ResourcePath("res/rom/service.rom"));
#endif
#endif
#ifndef NO_USE_DOS
	LoadRom(ROM_DOS,	xIo::ResourcePath("res/rom/dos513f.rom"));
#endif
#endif//USE_EMBEDDED_RESOURCES
#endif
}

#ifdef USE_BANKED_MEMORY_ACCESS
//=============================================================================
//	eRom::Reset
//-----------------------------------------------------------------------------
void eRom::Reset()
{
#ifndef NO_USE_128K
#ifndef NO_USE_SERVICE_ROM
	SelectPage(mode_48k ? ROM_48 : ROM_SYS);
#else
	SelectPage(ROM_SOS());
#endif
#else
	SelectPage(ROM_48);
#endif
}
#endif

#ifdef USE_MU
void eRom::init_rom_contents(eMemory::ePage memory_page, ePage rom_page) {
#if USE_COMPRESSED_ROM
	switch (rom_page) {
		case ROM_48:
			decompress_rom_to_memory(memory->Get(memory_page), "ROM_48");
			break;
#ifndef NO_USE_128K
		case ROM_128_0:
			decompress_rom_to_memory(memory->Get(memory_page), "ROM_128_0");
			break;
		case ROM_128_1:
			decompress_rom_to_memory(memory->Get(memory_page), "ROM_128_1");
			break;
#endif
		default:
			assert(false);
	}
#endif
}
#endif
#ifndef NO_USE_128K
#ifndef USE_HACKED_DEVICE_ABSTRACTION
//=============================================================================
//	eRom::IoWrite
//-----------------------------------------------------------------------------
bool eRom::IoWrite(word port) const
{
	return !mode_48k && !(port & 2) && !(port & 0x8000); // zx128 port
}
//=============================================================================
//	eRam::IoWrite
//-----------------------------------------------------------------------------
bool eRam::IoWrite(word port) const
{
	return !mode_48k && !(port & 2) && !(port & 0x8000); // zx128 port
}
#endif
//=============================================================================
//	eRom::IoWrite
//-----------------------------------------------------------------------------
void eRom::IoWrite(word port, byte v, int tact)
{
	SelectPage((ePage)((page_selected & ~1) + ((v >> 4) & 1)));
}
#endif

#ifdef USE_BANKED_MEMORY_ACCESS
//=============================================================================
//	eRam::Reset
//-----------------------------------------------------------------------------
void eRam::Reset()
{
	memory->SetPage(1, eMemory::P_RAM5);
	memory->SetPage(2, eMemory::P_RAM2);
	memory->SetPage(3, eMemory::P_RAM0);
}
#endif
#ifndef NO_USE_128K
//=============================================================================
//	eRam::IoWrite
//-----------------------------------------------------------------------------
void eRam::IoWrite(word port, byte v, int tact)
{
	int page = eMemory::P_RAM0 + (v & 7);
	memory->SetPage(3, page);
}
#endif
