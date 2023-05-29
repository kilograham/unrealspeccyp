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
#include "device.h"
#ifdef USE_KHAN_GPIO
// hack
#include "khan_lib.h"
#endif
//=============================================================================
//	eDevices::eDevices
//-----------------------------------------------------------------------------
eDevices::eDevices()
{
	memset(items, 0, sizeof(items));
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	memset(items_io_read, 0, sizeof(items_io_read));
	memset(items_io_write, 0, sizeof(items_io_write));
#endif
}
#ifndef NO_USE_DESTRUCTORS
//=============================================================================
//	eDevices::~eDevices
//-----------------------------------------------------------------------------
eDevices::~eDevices()
{
	for(int i = 0; i < D_COUNT; ++i)
	{
		SAFE_DELETE(items[i]);
	}
}
#endif
//=============================================================================
//	eDevices::Init
//-----------------------------------------------------------------------------
void eDevices::Init()
{
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	int io_read_count = 0;
	for(; items_io_read[io_read_count]; ++io_read_count);
	int io_write_count = 0;
	for(; items_io_write[io_write_count]; ++io_write_count);
	assert(io_read_count <= 8 && io_write_count <= 8); //only 8 devs max supported

	for(int port = 0; port < 0x10000; ++port)
	{
		byte devs = 0;
		for(int d = 0; d < io_read_count; ++d)
		{
			if(items_io_read[d]->IoRead(port))
				devs |= 1 << d;
		}
		io_read_map[port] = devs;
		devs = 0;
		for(int d = 0; d < io_write_count; ++d)
		{
			if(items_io_write[d]->IoWrite(port))
				devs |= 1 << d;
		}
		io_write_map[port] = devs;
	}

	int size = 1 << io_read_count;
	for(int i = 0; i < size; ++i)
	{
		eDevice** dl = io_read_cache[i];
		for(int d = 0; d < io_read_count; ++d)
		{
			if((byte)i&(1 << d))
				*dl++ = items_io_read[d];
		}
		*dl = NULL;
	}
	size = 1 << io_write_count;
	for(int i = 0; i < size; ++i)
	{
		eDevice** dl = io_write_cache[i];
		for(int d = 0; d < io_write_count; ++d)
		{
			if((byte)i&(1 << d))
				*dl++ = items_io_write[d];
		}
		*dl = NULL;
	}
#endif
}
//=============================================================================
//	eDevices::Reset
//-----------------------------------------------------------------------------
void eDevices::Reset()
{
	for(int i = 0; i < D_COUNT; ++i)
	{
		items[i]->Reset();
	}
}
//=============================================================================
//	eDevices::FrameStart
//-----------------------------------------------------------------------------
void eDevices::FrameStart(dword tacts)
{
	for(int i = 0; i < D_COUNT; ++i)
	{
		items[i]->FrameStart(tacts);
	}
}
//=============================================================================
//	eDevices::FrameUpdate
//-----------------------------------------------------------------------------
void eDevices::FrameUpdate()
{
	for(int i = 0; i < D_COUNT; ++i)
	{
		items[i]->FrameUpdate();
	}
}
//=============================================================================
//	eDevices::FrameEnd
//-----------------------------------------------------------------------------
void eDevices::FrameEnd(dword tacts)
{
	for(int i = 0; i < D_COUNT; ++i)
	{
		items[i]->FrameEnd(tacts);
	}
}
//=============================================================================
//	eDevices::_Add
//-----------------------------------------------------------------------------
void eDevices::_Add(eDeviceId id, eDevice* d)
{
	assert(d && !items[id]);
	d->Init();
	items[id] = d;
#ifndef USE_HACKED_DEVICE_ABSTRACTION
	if(d->IoNeed()&eDevice::ION_READ)
	{
		eDevice** dl = items_io_read;
		while(*dl)
			++dl;
		*dl = d;
	}
	if(d->IoNeed()&eDevice::ION_WRITE)
	{
		eDevice** dl = items_io_write;
		while(*dl)
			++dl;
		*dl = d;
	}
#endif
}

#ifdef USE_HACKED_DEVICE_ABSTRACTION
#include "../platform/platform.h"
#include "../speccy.h"
#ifndef NO_USE_TAPE
#include "input/tape.h"
#endif
#ifndef NO_USE_BEEPER
#include "sound/beeper.h"
#endif
#ifndef NO_USE_AY
#include "sound/ay.h"
#endif
#ifndef NO_USE_KEMPSTON
#include "input/kempston_joy.h"
#endif
#include "input/keyboard.h"
#include "ula.h"
#include "memory.h"

static struct {
	eUla *ula;
	eKeyboard *keyboard;
#ifndef NO_USE_TAPE
	eTape *tape;
#endif
#ifndef NO_USE_BEEPER
	eBeeper *beeper;
#endif
#ifndef NO_USE_128K
	eRom *rom;
	eRam *ram;
#endif
#ifndef NO_USE_AY
	eAY *ay;
#endif
#ifndef NO_USE_KEMPSTON
	eKempstonJoy *kempston_joy;
#endif
	bool cached;
} _devices;

static void cache_devices() {
	if (!_devices.cached) {
		_devices.ula = xPlatform::Handler()->Speccy()->Devices().Get<eUla>();
		_devices.keyboard = xPlatform::Handler()->Speccy()->Devices().Get<eKeyboard>();
#ifndef NO_USE_TAPE
		_devices.tape = xPlatform::Handler()->Speccy()->Devices().Get<eTape>();
#endif
#ifndef NO_USE_BEEPER
		_devices.beeper = xPlatform::Handler()->Speccy()->Devices().Get<eBeeper>();
#endif
#ifndef NO_USE_AY
		_devices.ay = xPlatform::Handler()->Speccy()->Devices().Get<eAY>();
#endif
#ifndef NO_USE_KEMPSTON
		_devices.kempston_joy = xPlatform::Handler()->Speccy()->Devices().Get<eKempstonJoy>();
#endif
#ifndef NO_USE_128K
		_devices.rom = xPlatform::Handler()->Speccy()->Devices().Get<eRom>();
		_devices.ram = xPlatform::Handler()->Speccy()->Devices().Get<eRam>();
#endif
		_devices.cached = true;
	}
}
extern "C" byte static_device_io_read(int port, int tact) {
	byte v = 0xff;
	cache_devices();
	if (port == 0xff) {
		_devices.ula->IoRead(port, &v, tact);
	} else if (!(port&1)) {
		_devices.keyboard->IoRead(port, &v, tact);
		// force press of enter
//        if (port == 49150) {
//            v = 190;
//        }
#ifndef NO_USE_TAPE
		_devices.tape->IoRead(port, &v, tact);
#endif
#ifdef USE_KHAN_GPIO
	} else if (0b11101011 == (port & 0xff)) {
		v = khan_gpio_read(port >> 8);
#endif
	}
#ifndef NO_USE_AY
	if  ((port & 0xC0FF) == 0xC0FD)
	{
		v = _devices.ay->Read();
	}
#endif
#ifndef NO_USE_KEMPSTON
	if (!(port&0x20)) {
		// A13,A15 not used in decoding
		uint p = port | 0xfa00;
		if (!(p == 0xfadf || p == 0xfbdf || p == 0xffdf)) {
			_devices.kempston_joy->IoRead(port, &v, tact);
		}
	}
#endif
#if !defined(NO_USE_FDD) || !defined(NO_USE_REPLAY)
#error todo
#endif
	return v;
}


// order of args important for assembler
extern "C" void static_device_io_write(byte v, int port, int tact) {
	cache_devices();
	if (!(port&1)) {
		_devices.ula->IoWrite(port, v, tact);
	}
#ifdef USE_KHAN_GPIO
	if (0b11101011 == (port & 0xff)) {
		khan_gpio_write(port >> 8, v);
	}
#endif
#ifndef NO_USE_BEEPER
	_devices.beeper->IoWrite(port, v, tact);
#endif
#if !defined(NO_USE_128K)
	if(!(port & 2)) {
		if (!(port & 0x8000)) // zx128 port
		{
			_devices.rom->IoWrite(port, v, tact);
			_devices.ram->IoWrite(port, v, tact);
			_devices.ula->SwitchScreen(!(v & 0x08), tact);
		}
#ifndef NO_USE_AY
		if  ((port & 0xC0FF) == 0xC0FD) {
			_devices.ay->Select(v);
		} else if ((port & 0xC000) == 0x8000) {
			_devices.ay->Write(tact, v);
		}
#endif
	}
#endif
}
#endif
