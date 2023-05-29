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

#ifndef	__DEVICE_H__
#define	__DEVICE_H__

#pragma once

//*****************************************************************************
//	eDevice
//-----------------------------------------------------------------------------
class eDevice
{
public:
	eDevice() {}
#ifndef NO_USE_DESTRUCTORS
	virtual ~eDevice() {}
#endif
	virtual void Init() {}
	virtual void Reset() {}
	virtual void FrameStart(dword tacts) {}
	virtual void FrameUpdate() {}
	virtual void FrameEnd(dword tacts) {}

#ifndef USE_HACKED_DEVICE_ABSTRACTION
	enum eIoNeed { ION_READ = 0x01, ION_WRITE = 0x02 };
	virtual bool IoRead(word port) const { return false; }
	virtual bool IoWrite(word port) const { return false; }
	virtual dword IoNeed() const { return 0; }
#endif
	virtual void IoRead(word port, byte* v, int tact) {}
	virtual void IoWrite(word port, byte v, int tact) {}
};

enum eDeviceId {
	D_ROM,
	D_RAM,
	D_ULA,
	D_KEYBOARD,
#ifndef NO_USE_KEMPSTON
	D_KEMPSTON_JOY,
#ifndef USE_MU
	D_KEMPSTON_MOUSE,
#endif
#endif
#ifndef NO_USE_BEEPER
	D_BEEPER,
#endif
#ifndef NO_USE_AY
	D_AY,
#endif
#ifndef NO_USE_FDD
	D_WD1793,
#endif
#ifndef NO_USE_TAPE
	D_TAPE,
#endif
	D_COUNT
};

#ifdef USE_HACKED_DEVICE_ABSTRACTION
extern "C" byte static_device_io_read(int port, int tact);
extern "C" void static_device_io_write(byte v, int port, int tact);
#endif
//*****************************************************************************
//	eDevices
//-----------------------------------------------------------------------------
class eDevices
{
public:
	eDevices();

#ifndef NO_USE_DESTRUCTORS
	~eDevices();
#endif

	void Init();
	void Reset();

	template<class T> void Add(T* d) { _Add(T::Id(), d); }
	template<class T> T* Get() const { return (T*)_Get(T::Id()); }

	byte IoRead(word port, int tact)
	{
#ifndef USE_HACKED_DEVICE_ABSTRACTION
		byte v = 0xff;
		eDevice** dl = io_read_cache[io_read_map[port]];
		while(*dl)
			(*dl++)->IoRead(port, &v, tact);
		return v;
#else
		return static_device_io_read(port, tact);
#endif
	}
	void IoWrite(word port, byte v, int tact)
	{
#ifndef USE_HACKED_DEVICE_ABSTRACTION
		eDevice** dl = io_write_cache[io_write_map[port]];
		while(*dl)
			(*dl++)->IoWrite(port, v, tact);
#else
		static_device_io_write(v, port, tact);
#endif
	}

	void FrameStart(dword tacts);
	void FrameUpdate();
	void FrameEnd(dword tacts);

protected:
	void _Add(eDeviceId id, eDevice* d);
	eDevice* _Get(eDeviceId id) const { return items[id]; }
	eDevice* items[D_COUNT];
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	eDevice* items_io_read[D_COUNT + 1];
	eDevice* items_io_write[D_COUNT + 1];
	byte io_read_map[0x10000];
	byte io_write_map[0x10000];
	eDevice* io_read_cache[0x100][9];
	eDevice* io_write_cache[0x100][9];
#endif
};

#endif//__DEVICE_H__
