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

#ifndef	__MEMORY_H__
#define	__MEMORY_H__

#include "device.h"

#pragma once

#undef PAGE_SIZE

#if !defined(NO_USE_128K)
#ifdef USE_SINGLE_64K_MEMORY
#error cant use 64k memory for 128
#endif
#if !defined(USE_BANKED_MEMORY_ACCESS)
#define USE_BANKED_MEMORY_ACCESS
#endif
#endif

//*****************************************************************************
//	eMemory
//-----------------------------------------------------------------------------
class eMemory
{
public:
	eMemory();
#ifndef NO_USE_DESTRUCTORS
	virtual ~eMemory();
#endif
	byte Read(word addr) const
	{
#ifndef USE_SINGLE_64K_MEMORY
		byte* a = bank_read[(addr >> 14) & 3] + (addr & 0xffffu);
		return *a;
#else
		return memory[addr];
#endif
	}
	void Write(word addr, byte v)
	{
#ifndef USE_SINGLE_64K_MEMORY
		byte* a = bank_write[(addr >> 14) & 3];
		if(!a) //rom write prevent
			return;
		a += addr & 0xffffu;
		*a = v;
#else
		if (addr>>14) {
			memory[addr] = v;
		}
#endif
	}
	byte* Get(int page) { return memory + page * PAGE_SIZE; }

	enum ePage
	{
#ifndef NO_USE_128K
#ifndef USE_OVERLAPPED_ROMS
		P_ROM0 = 0,
		P_ROM1,
#ifndef NO_USE_SERVICE_ROM
		P_ROM2,
#endif
#ifndef NO_USE_DOS
		P_ROM3,
#endif
#ifndef USE_OVERLAPPED_ROMS
		P_ROM4,
#endif
#else
		P_ROM_ALT0 = 0, P_ROM_ALT1,
#endif
		P_RAM0, P_RAM1, P_RAM2, P_RAM3,
		P_RAM4, P_RAM5, P_RAM6, P_RAM7,
#else
		P_ROM4 = 0, P_RAM5, P_RAM2, P_RAM0,
#endif
		P_AMOUNT
	};
	void SetPage(int idx, int page);
	int	Page(int idx);
	enum { BANKS_AMOUNT = 4, PAGE_SIZE = 0x4000, SIZE = P_AMOUNT * PAGE_SIZE };
#ifdef USE_BANKED_MEMORY_ACCESS
	byte **GetBankReads() { return bank_read; }
	byte **GetBankWrites() { return bank_write; }
#endif
protected:
#ifdef USE_BANKED_MEMORY_ACCESS
	byte* bank_read[BANKS_AMOUNT];
	byte* bank_write[BANKS_AMOUNT];
#endif
	byte* memory;
};

//*****************************************************************************
//	eRom
//-----------------------------------------------------------------------------
class eRom : public eDevice
{
public:
	eRom(eMemory* m) : memory(m), page_selected((ePage)-1), mode_48k(false) {}

	virtual void Init();
#ifdef USE_BANKED_MEMORY_ACCESS
	virtual void Reset();
#endif
#ifndef NO_USE_128K
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoWrite(word port) const;
	virtual dword IoNeed() const { return ION_WRITE; }
#endif
	virtual void IoWrite(word port, byte v, int tact);
#endif

	enum ePage
	{
#ifndef USE_OVERLAPPED_ROMS
#if !defined(NO_USE_128K)
		ROM_128_1	= eMemory::P_ROM0,
		ROM_128_0	= eMemory::P_ROM1,
#endif
#ifndef NO_USE_128K
#ifndef NO_USE_SERVICE_ROM
		ROM_SYS		= eMemory::P_ROM2,
#endif
#endif
#ifndef NO_USE_DOS
		ROM_DOS		= eMemory::P_ROM3,
#endif
		ROM_48		= eMemory::P_ROM4,
#else
		ROM_128_1 = (eMemory::P_AMOUNT + 1) & ~1, // must be even
		ROM_128_0,
		ROM_48,
#endif
	};

	void Read(word addr)
	{
#ifndef NO_USE_DOS
		byte pc_h = addr >> 8;
		if(page_selected == ROM_SOS() && (pc_h == 0x3d))
		{
			SelectPage(ROM_DOS);
		}
		else if(DosSelected() && (pc_h & 0xc0)) // pc > 0x3fff closes tr-dos
		{
			SelectPage(ROM_SOS());
		}
#endif
	}
	bool DosSelected() const {
#ifndef NO_USE_DOS
		return page_selected == ROM_DOS;
#else
		return false;
#endif
	}
#ifndef NO_USE_128K
	void Mode48k(bool on) { mode_48k = on; }
	ePage ROM_SOS() const { return mode_48k ? ROM_48 : ROM_128_0; }
#else
	ePage ROM_SOS() const { return ROM_48; }
#endif

	static eDeviceId Id() { return D_ROM; }
#ifdef USE_MU
	void init_rom_contents(eMemory::ePage memory_page, ePage rom_page);
#endif
	void SelectPage(ePage page) {
#ifdef USE_BANKED_MEMORY_ACCESS
#ifdef USE_OVERLAPPED_ROMS
		if (page != page_selected) {
			if ((page & ~1) != (page_selected & ~1)) {
//                printf("Switching ROM contents for %d\n", page);
				switch (page) {
					case ROM_128_1:
					case ROM_128_0:
						init_rom_contents(eMemory::ePage::P_ROM_ALT0, ROM_128_1);
						init_rom_contents(eMemory::ePage::P_ROM_ALT1, ROM_128_0);
						break;
					case ROM_48:
						init_rom_contents(eMemory::ePage::P_ROM_ALT0, ROM_48);
						break;
					default:
						assert(false);

				}
			}
//            printf("Switching ROM page to %d\n", page);
			page_selected = page;
			memory->SetPage(0, page & 1 ? eMemory::ePage::P_ROM_ALT1 : eMemory::ePage::P_ROM_ALT0);
		}
#else
		assert(page < eMemory::P_RAM0);
		page_selected = page; memory->SetPage(0, page_selected);
#endif
#else
		assert(page == ROM_48);
		page_selected = page;
#endif
	}
protected:
	void LoadRom(int page, const char* rom);

protected:
	eMemory* memory;
	ePage page_selected;
	bool mode_48k;
};

//*****************************************************************************
//	eRam
//-----------------------------------------------------------------------------
class eRam : public eDevice
{
public:
	eRam(eMemory* m) : memory(m), mode_48k(false) {}
#ifdef USE_BANKED_MEMORY_ACCESS
	virtual void Reset();
#endif
#ifndef NO_USE_128K
	void Mode48k(bool on) { mode_48k = on; }
	virtual void IoWrite(word port, byte v, int tact);
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoWrite(word port) const;
	virtual dword IoNeed() const { return ION_WRITE; }
#endif
#endif
	bool Mode48k() const { return mode_48k; }
	static eDeviceId Id() { return D_RAM; }

protected:
	eMemory* memory;
	bool mode_48k;
};

#endif//__MEMORY_H__
