/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2015 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
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

#include "platform/platform.h"
#include "speccy.h"
#include "devices/memory.h"
#include "devices/ula.h"
#include "devices/input/keyboard.h"
#ifndef NO_USE_KEMPSTON
#include "devices/input/kempston_joy.h"
#ifndef USE_MU
#include "devices/input/kempston_mouse.h"
#endif
#endif
#ifndef NO_USE_TAPE
#include "devices/input/tape.h"
#endif
#include "devices/sound/device_sound.h"
#ifndef NO_USE_AY
#include "devices/sound/ay.h"
#endif
#ifndef NO_USE_BEEPER
#include "devices/sound/beeper.h"
#endif
#ifndef NO_USE_FDD
#include "devices/fdd/wd1793.h"
#endif
#include "z80/z80.h"
#include "snapshot/snapshot.h"
#include "platform/io.h"
#ifdef USE_UI
#include "../ui/ui_desktop.h"
#include "../platform/custom_ui/ui_main.h"
#endif
#include "tools/profiler.h"
#include "tools/options.h"
#include "tools/io_select.h"
#include "snapshot/rzx.h"
#include "options_common.h"
#include "file_type.h"
#ifdef USE_STREAM
extern "C" {
#include "stream.h"
};
#endif

namespace xPlatform
{

class eMacro
{
public:
	eMacro() : frame(-1) {}
	virtual ~eMacro() {}
	virtual bool Do() = 0;
	virtual bool Update()
	{
		++frame;
		return Do();
	}
protected:
	int frame;
};

static struct eSpeccyHandler : public eHandler
#ifndef NO_USE_REPLAY
		, public eRZX::eHandler
#endif
#ifndef USE_HACKED_DEVICE_ABSTRACTION
		,public xZ80::eZ80::eHandlerIo
#endif
{
	eSpeccyHandler() : speccy(NULL), macro(NULL), replay(NULL), video_paused(0), video_frame(-1), inside_replay_update(false) {}
#ifndef NO_USE_DESTRUCTORS
	virtual ~eSpeccyHandler() { assert(!speccy); }
#endif
	virtual void OnInit();
	virtual void OnDone();
	virtual const char* OnLoop();
#ifndef NO_USE_SCREEN
	virtual void* VideoData() const { return speccy->Device<eUla>()->Screen(); }
#endif
	virtual void* VideoDataUI() const
	{
#ifdef USE_UI
		return ui_desktop->VideoData();
#else//USE_UI
		return NULL;
#endif//USE_UI
	}
#ifndef USE_MU_SIMPLIFICATIONS
	virtual const char* WindowCaption() const { return "Unreal Speccy Portable"; }
#endif
	virtual void OnKey(char key, dword flags);
#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
	virtual void OnMouse(eMouseAction action, byte a, byte b);
#endif
#endif
	virtual bool FileTypeSupported(const char* name) const {
		const eFileType* t = eFileType::FindByName(name);
		return t && t->AbleOpen();
	}
	virtual bool OnOpenFile(const char* name, const void* data, size_t data_size);
#ifdef USE_STREAM
	virtual bool OnOpenFileCompressed(const char* name, const void* comressed_data, size_t compressed_data_size, size_t data_size);
#endif
	bool OpenFile(const char* name, const void* data, size_t data_size);
#ifndef NO_USE_SAVE
	virtual bool OnSaveFile(const char* name);
#endif
	virtual eActionResult OnAction(eAction action);

#ifndef USE_MU
	virtual int	AudioSources() const { return FullSpeed() ? 0 : SOUND_DEV_COUNT; }
	virtual void* AudioData(int source) const { return sound_dev[source]->AudioData(); }
	virtual dword AudioDataReady(int source) const { return sound_dev[source]->AudioDataReady(); }
	virtual void AudioDataUse(int source, dword size) { sound_dev[source]->AudioDataUse(size); }
	virtual void AudioSetSampleRate(dword sample_rate);
#endif
	virtual void VideoPaused(bool paused) {	paused ? ++video_paused : --video_paused; }
	virtual bool IsVideoPaused() {
		return video_paused;
	}
#ifndef USE_MU_SIMPLIFICATIONS
	virtual int VideoFrame() const { return video_frame; }
#endif

	virtual bool FullSpeed() const final {
#ifndef NO_USE_FAST_TAPE
		return speccy->CPU()->HandlerStep() != NULL;
#else
		return false;
#endif
	}

	virtual eSpeccy* Speccy() const { return speccy; }

	void PlayMacro(eMacro* m) { SAFE_DELETE(macro); macro = m; }

#ifndef NO_USE_REPLAY
	virtual bool RZX_OnOpenSnapshot(const char* name, const void* data, size_t data_size) { return OpenFile(name, data, data_size); }
#endif

	virtual byte Z80_IoRead(word port, int tact)
	{
		byte r = 0xff;
		// todo if not replaying there should be no handler.io
#ifndef NO_USE_REPLAY
		replay->IoRead(&r);
#endif
		return r;
	}

#ifndef NO_USE_REPLAY
	virtual bool GetReplayProgress(dword* frame_current, dword* frames_total, dword* frames_cached)
	{
		if(replay)
			return replay->GetProgress(frame_current, frames_total, frames_cached) == eRZX::E_OK;
		return false;
	}
#endif

#ifndef NO_USE_RZX
	const char* RZXErrorDesc(eRZX::eError err) const;
#endif

#ifndef NO_USE_REPLAY
	void Replay(eRZX* r)
	{
		speccy->CPU()->HandlerIo(NULL);
		SAFE_DELETE(replay);
		replay = r;
		if(replay)
			speccy->CPU()->HandlerIo(this);
	}
#endif

	eSpeccy* speccy;
#ifdef USE_UI
	xUi::eDesktop* ui_desktop;
#endif//USE_UI
	eMacro* macro;
	eRZX* replay;
	int video_paused;
	int video_frame;
	bool inside_replay_update;

	enum { SOUND_DEV_COUNT = 3 };
	eDeviceSound* sound_dev[SOUND_DEV_COUNT];
} sh;

void eSpeccyHandler::OnInit()
{
	assert(!speccy);
	speccy = new eSpeccy;
#ifdef USE_UI
	ui_desktop = new xUi::eDesktop;
	ui_desktop->Insert(new xUi::eMainDialog);
#endif//USE_UI
#ifndef NO_USE_BEEPER
	sound_dev[0] = speccy->Device<eBeeper>();
#endif
#ifndef NO_USE_AY
	sound_dev[1] = speccy->Device<eAY>();
#endif
#ifndef NO_USE_TAPE
	sound_dev[2] = speccy->Device<eTape>();
#endif
	xOptions::Load();
	OnAction(A_RESET);
}
void eSpeccyHandler::OnDone()
{
	xOptions::Store();
	SAFE_DELETE(macro);
#ifndef NO_USE_REPLAY
	SAFE_DELETE(replay);
#endif
#ifndef NO_USE_DESTRUCTORS
	SAFE_DELETE(speccy);
#endif
#ifdef USE_UI
	SAFE_DELETE(ui_desktop);
#endif//USE_UI
	PROFILER_DUMP;
}
const char* eSpeccyHandler::OnLoop()
{
	const char* error = NULL;
	if(FullSpeed() || !video_paused)
	{
		if(macro)
		{
			if(!macro->Update())
				SAFE_DELETE(macro);
		}
#ifndef NO_USE_REPLAY
		if(replay)
		{
			int icount = 0;
			inside_replay_update = true;
			eRZX::eError err = replay->Update(&icount);
			inside_replay_update = false;
			if(err == eRZX::E_OK)
			{
				speccy->Update(&icount);
				err = replay->CheckSync();
			}
			if(err != eRZX::E_OK)
			{
				Replay(NULL);
				error = RZXErrorDesc(err);
			}
		}
		else
#endif
			speccy->Update(NULL);
		++video_frame;
	}
#ifdef USE_UI
	ui_desktop->Update();
#endif//USE_UI
	return error;
}
const char* eSpeccyHandler::RZXErrorDesc(eRZX::eError err) const
{
	switch(err)
	{
	case eRZX::E_OK:			return "rzx_ok";
	case eRZX::E_FINISHED:		return "rzx_finished";
	case eRZX::E_SYNC_LOST:		return "rzx_sync_lost";
	case eRZX::E_INVALID:		return "rzx_invalid";
	case eRZX::E_UNSUPPORTED:	return "rzx_unsupported";
	}
	return NULL;
}
void eSpeccyHandler::OnKey(char key, dword flags)
{
	bool down = (flags&KF_DOWN) != 0;
	bool shift = (flags&KF_SHIFT) != 0;
	bool ctrl = (flags&KF_CTRL) != 0;
	bool alt = (flags&KF_ALT) != 0;

#ifdef USE_UI
	if(!(flags&KF_UI_SENDER))
	{
		ui_desktop->OnKey(key, flags);
		if(ui_desktop->Focused())
			return;
	}
#endif//USE_UI

#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
	if(flags&KF_KEMPSTON)
		speccy->Device<eKempstonJoy>()->OnKey(key, down);
#endif
#endif
	if(flags&KF_CURSOR)
	{
		switch(key)
		{
		case 'l' : key = '5'; shift = down; break;
		case 'r' : key = '8'; shift = down; break;
		case 'u' : key = '7'; shift = down; break;
		case 'd' : key = '6'; shift = down; break;
		case 'f' : key = '0'; shift = false; break;
		}
	}
	else if(flags&KF_QAOP)
	{
		switch(key)
		{
		case 'l' : key = 'O'; break;
		case 'r' : key = 'P'; break;
		case 'u' : key = 'Q'; break;
		case 'd' : key = 'A'; break;
		case 'f' : key = ' '; break;
		}
	}
	else if(flags&KF_SINCLAIR2)
	{
		switch(key)
		{
		case 'l' : key = '6'; break;
		case 'r' : key = '7'; break;
		case 'u' : key = '9'; break;
		case 'd' : key = '8'; break;
		case 'f' : key = '0'; break;
		}
	}
	speccy->Device<eKeyboard>()->OnKey(key, down, shift, ctrl, alt);
}
#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
void eSpeccyHandler::OnMouse(eMouseAction action, byte a, byte b)
{
	switch(action)
	{
	case MA_MOVE: 	speccy->Device<eKempstonMouse>()->OnMouseMove(a, b); 	break;
	case MA_BUTTON:	speccy->Device<eKempstonMouse>()->OnMouseButton(a, b != 0);	break;
	default: break;
	}
}
#endif
#endif
bool eSpeccyHandler::OnOpenFile(const char* name, const void* data, size_t data_size)
{
#ifndef USE_MU_SIMPLIFICATIONS
	OpLastFile(name);
#endif
	return OpenFile(name, data, data_size);
}

#ifdef USE_STREAM
static struct stream *open_stream(const void *data, size_t data_size) {
	struct stream *stream;
#if !PICO_ON_DEVICE
	// copy because the data may not be owned
	byte *data_owned = new byte[data_size];
	memcpy(data_owned, data, data_size);
	data = data_owned;
#endif
#ifndef USE_XIP_STREAM
#if !PICO_ON_DEVICE
	bool owned = true;
#else
	bool owned = false;
#endif
	stream = memory_stream_open((const uint8_t *)data, data_size, owned);
#else
	stream = xip_stream_open((const uint8_t *)data, data_size, 512, 10);
#endif
	return stream;
}

bool eSpeccyHandler::OnOpenFileCompressed(const char* name, const void* comressed_data, size_t compressed_data_size, size_t data_size) {
	const eFileType* t = eFileType::FindByName(name);
	if(!t)
		return false;

	struct stream *underlying = open_stream(comressed_data, compressed_data_size);
	struct stream *compressed_stream = compressed_stream_open(underlying, data_size);
	return t->Open(compressed_stream);
}
#endif

bool eSpeccyHandler::OpenFile(const char* name, const void* data, size_t data_size)
{
	const eFileType* t = eFileType::FindByName(name);
	if(!t)
		return false;

	if(data && data_size) {
#ifndef USE_STREAM
		return t->Open(data, data_size);
#else
		return t->Open(open_stream(data, data_size));
#endif
	}

	return t->Open(name);
}
#ifndef NO_USE_SAVE
bool eSpeccyHandler::OnSaveFile(const char* name)
{
	OpLastFile(name);
	const eFileType* t = eFileType::FindByName(name);
	if(!t)
		return false;

	char path[xIo::MAX_PATH_LEN];
	strcpy(path, name);
	int l = strlen(path);
	char* e = path + l;
	while(e > path && *e != '\\' && *e != '/')
		--e;
	*e = 0;
	if(xIo::PathCreate(path))
		return t->Store(name);
	return false;
}
#endif
#ifndef NO_USE_FAST_TAPE
static struct eOptionTapeFast : public xOptions::eOptionBool
{
	eOptionTapeFast() { Set(true); }
	virtual const char* Name() const {
#ifndef USE_MU
		return "fast tape";
#else
		return "Fast tape";
#endif
	}
	virtual int Order() const { return 50; }
} op_tape_fast;
#endif

static struct eOptionAutoPlayImage : public xOptions::eOptionBool
{
	eOptionAutoPlayImage() { Set(true); }
	virtual const char* Name() const {
#ifndef USE_MU
		return "auto play image";
#else
		return "Autoplay image";
#endif
	}
	virtual int Order() const { return 55; }
} op_auto_play_image;

#ifndef NO_USE_128K
static struct eOption48K : public xOptions::eOptionBoolWithPending
{
	virtual const char* Name() const {
#ifndef USE_MU
		return "mode 48k";
#else
		return "48K mode";
#endif
	}
	virtual void Change(bool next = true)
	{
		eOptionBool::Change();
#ifndef USE_MU
		Apply();
#endif
	}

	virtual void Apply()
	{
		sh.OnAction(A_RESET);
	}
	virtual int Order() const { return 65; }
} op_48k;

#ifndef NO_USE_SERVICE_ROM
static struct eOptionResetToServiceRom : public xOptions::eOptionBool
{
	virtual const char* Name() const { return "reset to service rom"; }
	virtual int Order() const { return 79; }
} op_reset_to_service_rom;
#endif
#endif

eActionResult eSpeccyHandler::OnAction(eAction action)
{
	switch(action)
	{
	case A_RESET:
#ifndef NO_USE_REPLAY
		if(!inside_replay_update) // can be called from replay->Update()
			SAFE_DELETE(replay);
#endif
		SAFE_DELETE(macro);
#ifndef NO_USE_128K
		speccy->Mode48k(op_48k);
#endif
		speccy->Reset();
#ifndef NO_USE_128K
		if(!speccy->Mode48k())
		{
#ifndef NO_USE_SERVICE_ROM
			speccy->Device<eRom>()->SelectPage(op_reset_to_service_rom ? eRom::ROM_SYS : eRom::ROM_128_1);
#else
			speccy->Device<eRom>()->SelectPage(eRom::ROM_128_1);
#endif
		}
#endif
#ifndef USE_HACKED_DEVICE_ABSTRACTION
		if(inside_replay_update)
			speccy->CPU()->HandlerIo(this);
#endif
		return AR_OK;
#ifndef NO_USE_TAPE
	case A_TAPE_TOGGLE:
		{
			eTape* tape = speccy->Device<eTape>();
			if(!tape->Inserted())
				return AR_TAPE_NOT_INSERTED;
			if(!tape->Started())
			{
#ifndef NO_USE_FAST_TAPE
				if(op_tape_fast)
					speccy->CPU()->HandlerStep(fast_tape_emul);
				else
					speccy->CPU()->HandlerStep(NULL);
#endif
				tape->Start();
			}
			else
				tape->Stop();
			return tape->Started() ? AR_TAPE_STARTED : AR_TAPE_STOPPED;
		}
	case A_TAPE_REWIND:
	{
		eTape *tape = speccy->Device<eTape>();
		if(!tape->Inserted())
			return AR_TAPE_NOT_INSERTED;
		tape->Rewind();
		return tape->Started() ? AR_TAPE_STARTED : AR_TAPE_STOPPED;
	}
	case A_TAPE_QUERY:
		{
			eTape* tape = speccy->Device<eTape>();
			if(!tape->Inserted())
				return AR_TAPE_NOT_INSERTED;
			return tape->Started() ? AR_TAPE_STARTED : AR_TAPE_STOPPED;
		}
#endif
#ifndef NO_USE_FDD
	case A_DISK_QUERY:
		{
			eWD1793* wd1793 = speccy->Device<eWD1793>();
			return wd1793->DiskChanged(OpDrive()) ? AR_DISK_CHANGED : AR_DISK_NOT_CHANGED;
		}
#endif
	}
	return AR_ERROR;
}

#ifndef USE_MU
void eSpeccyHandler::AudioSetSampleRate(dword sample_rate)
{
#ifndef NO_USE_AY
	speccy->Device<eAY>()->SetTimings(SNDR_DEFAULT_SYSTICK_RATE, SNDR_DEFAULT_AY_RATE, sample_rate);
#endif
#ifndef NO_USE_BEEPER
	speccy->Device<eBeeper>()->SetTimings(SNDR_DEFAULT_SYSTICK_RATE, sample_rate);
#endif
#ifndef NO_USE_TAPE
	speccy->Device<eTape>()->SetTimings(SNDR_DEFAULT_SYSTICK_RATE, sample_rate);
#endif
}
#endif

#ifndef NO_USE_AY
static void SetupSoundChip();
static struct eOptionSoundChip : public xOptions::eOptionInt
{
	eOptionSoundChip() { Set(SC_AY); }
	enum eType { SC_FIRST, SC_AY = SC_FIRST, SC_YM, SC_LAST };
#ifndef USE_MU
	virtual const char* Name() const { return "sound chip"; }
#else
	virtual const char* Name() const { return "Sound Chip"; }
#endif
	virtual const char** Values() const
	{
#ifndef USE_MU
		static const char* values[] = { "ay", "ym", NULL };
#else
		static const char* values[] = { "AY", "YM", NULL };
#endif
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(SC_FIRST, SC_LAST, next);
		Apply();
	}
	virtual void Apply()
	{
		SetupSoundChip();
	}
	virtual int Order() const { return 33; }
}op_sound_chip;

#ifndef USE_FAST_AY
static struct eOptionAYStereo : public xOptions::eOptionInt
{
	eOptionAYStereo() { Set(AS_ABC); }
	enum eMode { AS_FIRST, AS_ABC = AS_FIRST, AS_ACB, AS_BAC, AS_BCA, AS_CAB, AS_CBA, AS_MONO, AS_LAST };
	virtual const char* Name() const { return "ay stereo"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "abc", "acb", "bac", "bca", "cab", "cba", "mono", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(AS_FIRST, AS_LAST, next);
		Apply();
	}
	virtual void Apply()
	{
		SetupSoundChip();
	}
	virtual int Order() const { return 25; }
}op_ay_stereo;
#endif

void SetupSoundChip()
{
	eOptionSoundChip::eType chip = (eOptionSoundChip::eType)(int)op_sound_chip;
	eAY* ay = sh.speccy->Device<eAY>();
#ifndef USE_FAST_AY
	eOptionAYStereo::eMode stereo = (eOptionAYStereo::eMode)(int)op_ay_stereo;
	const SNDCHIP_PANTAB* sndr_pan = SNDR_PAN_MONO;
	switch(stereo)
	{
	case eOptionAYStereo::AS_ABC: sndr_pan = SNDR_PAN_ABC; break;
	case eOptionAYStereo::AS_ACB: sndr_pan = SNDR_PAN_ACB; break;
	case eOptionAYStereo::AS_BAC: sndr_pan = SNDR_PAN_BAC; break;
	case eOptionAYStereo::AS_BCA: sndr_pan = SNDR_PAN_BCA; break;
	case eOptionAYStereo::AS_CAB: sndr_pan = SNDR_PAN_CAB; break;
	case eOptionAYStereo::AS_CBA: sndr_pan = SNDR_PAN_CBA; break;
	case eOptionAYStereo::AS_MONO: sndr_pan = SNDR_PAN_MONO; break;
	case eOptionAYStereo::AS_LAST: break;
	}
#endif
	ay->SetChip(chip == eOptionSoundChip::SC_AY ? eAY::CHIP_AY : eAY::CHIP_YM);
#ifndef USE_FAST_AY
	ay->SetVolumes(0x7FFF, chip == eOptionSoundChip::SC_AY ? SNDR_VOL_AY : SNDR_VOL_YM, sndr_pan);
#endif
}
#endif

#ifndef NO_USE_REPLAY
static struct eFileTypeRZX : public eFileType
{
	virtual bool Open(const void* data, size_t data_size) const
	{
		eRZX* rzx = new eRZX;
		if(rzx->Open(data, data_size, &sh) == eRZX::E_OK)
		{
			sh.Replay(rzx);
			return true;
		}
		else
		{
			sh.Replay(NULL);
			SAFE_DELETE(rzx);
		}
		return false;
	}
	virtual const char* Type() const { return "rzx"; }
} ft_rzx;
#endif
#ifndef NO_USE_Z80
static struct eFileTypeZ80 : public eFileType
{
#ifndef USE_STREAM
	virtual bool Open(const void* data, size_t data_size) const
	{
		sh.OnAction(A_RESET);
		return xSnapshot::Load(sh.speccy, Type(), data, data_size);
	}
#else
	virtual bool Open(struct stream *stream) const
	{
		sh.OnAction(A_RESET);
		// to save memory we close tape
		sh.speccy->Device<eTape>()->CloseTape();
		// todo hack ... because we only have one inflator, the A_RESET above can mix in the old stream, so we reset again.
		stream_reset(stream);
		return xSnapshot::Load(sh.speccy, Type(), stream);
	}
#endif
	virtual const char* Type() const { return "z80"; }
} ft_z80;
#endif
#ifndef NO_USE_SZX
static struct eFileTypeSZX : public eFileTypeZ80
{
	virtual const char* Type() const { return "szx"; }
} ft_szx;
#endif
#ifndef NO_USE_SNA
static struct eFileTypeSNA : public eFileTypeZ80
{
#ifndef NO_USE_SAVE
	virtual bool Store(const char* name) const
	{
		return xSnapshot::Store(sh.speccy, name);
	}
#endif
	virtual const char* Type() const { return "sna"; }
} ft_sna;
#endif
class eMacroDiskRunBootable : public eMacro
{
	virtual bool Do()
	{
		switch(frame)
		{
		case 70:	sh.OnKey('7', KF_DOWN|KF_SHIFT|KF_UI_SENDER);	break;
		case 72:	sh.OnKey('7', KF_UI_SENDER);					break;
		case 100:	sh.OnKey('e', KF_DOWN|KF_UI_SENDER);			break;
		case 102:	sh.OnKey('e', KF_UI_SENDER);					break;
		case 140:	sh.OnKey('R', KF_DOWN|KF_UI_SENDER);			break;
		case 142:	sh.OnKey('R', KF_UI_SENDER);					break;
		case 160:	sh.OnKey('e', KF_DOWN|KF_UI_SENDER);			break;
		case 162:	sh.OnKey('e', KF_UI_SENDER);
			return false;
		}
		return true;
	}
};

class eMacroDiskRunWithMaxPetrov : public eMacro
{
	virtual bool Do()
	{
		switch(frame)
		{
		case 100:	sh.OnKey('e', KF_DOWN|KF_UI_SENDER);			break;
		case 102:	sh.OnKey('e', KF_UI_SENDER);					break;
		case 200:	sh.OnKey('e', KF_DOWN|KF_UI_SENDER);			break;
		case 202:	sh.OnKey('e', KF_UI_SENDER);
			return false;
		}
		return true;
	}
};

#ifndef NO_USE_FDD
static struct eFileTypeTRD : public eFileType
{
	virtual bool Open(const void* data, size_t data_size) const
	{
		eWD1793* wd = sh.speccy->Device<eWD1793>();
		bool ok = wd->Open(Type(), OpDrive(), data, data_size);
		if(ok && op_auto_play_image)
		{
			sh.OnAction(A_RESET);
			if(sh.speccy->Mode48k())
			{
				sh.speccy->Device<eRom>()->SelectPage(eRom::ROM_DOS);
			}
			else
			{
				if(wd->Bootable(OpDrive()))
				{
					sh.speccy->Device<eRom>()->SelectPage(eRom::ROM_128_1);
					sh.PlayMacro(new eMacroDiskRunBootable);
				}
				else
				{
					sh.speccy->Device<eRom>()->SelectPage(eRom::ROM_SYS);
					sh.PlayMacro(new eMacroDiskRunWithMaxPetrov);
				}
			}
		}
		return ok;
	}
	virtual bool Store(const char* name) const
	{
		FILE* file = fopen(name, "wb");
		if(!file)
			return false;
		eWD1793* wd = sh.speccy->Device<eWD1793>();
		bool ok = wd->Store(Type(), OpDrive(), file);
		fclose(file);
		return ok;
	}
	virtual const char* Type() const { return "trd"; }
} ft_trd;
static struct eFileTypeSCL : public eFileTypeTRD
{
	virtual const char* Type() const { return "scl"; }
} ft_scl;
static struct eFileTypeFDI : public eFileTypeTRD
{
	virtual const char* Type() const { return "fdi"; }
} ft_fdi;
static struct eFileTypeUDI : public eFileTypeTRD
{
	virtual const char* Type() const { return "udi"; }
} ft_udi;
static struct eFileTypeTD0 : public eFileTypeTRD
{
	virtual const char* Type() const { return "td0"; }
} ft_td0;
#endif

class eMacroTapeLoad : public eMacro
{
	virtual bool Do()
	{
		switch(frame)
		{
		case 100:
			sh.OnKey('J', KF_DOWN|KF_UI_SENDER);
			break;
		case 102:
			sh.OnKey('J', KF_UI_SENDER);
			sh.OnKey('P', KF_DOWN|KF_ALT|KF_UI_SENDER);
			break;
		case 104:
			sh.OnKey('P', KF_UI_SENDER);
			break;
		case 110:
			sh.OnKey('P', KF_DOWN|KF_ALT|KF_UI_SENDER);
			break;
		case 112:
			sh.OnKey('P', KF_UI_SENDER);
			break;
		case 120:
			sh.OnKey('e', KF_DOWN|KF_UI_SENDER);
			break;
		case 122:
			sh.OnKey('e', KF_UI_SENDER);
#ifndef NO_USE_TAPE
			sh.OnAction(A_TAPE_TOGGLE);
#endif
			return false;
		}
		return true;
	}
};

#ifndef NO_USE_TAPE
static struct eFileTypeTAP : public eFileType
{
#ifndef USE_STREAM
	virtual bool Open(const void* data, size_t data_size) const
	{
		bool ok = sh.speccy->Device<eTape>()->Open(Type(), data, data_size);
		if(ok && op_auto_play_image)
		{
			sh.OnAction(A_RESET);
			sh.speccy->Devices().Get<eRom>()->SelectPage(sh.speccy->Devices().Get<eRom>()->ROM_SOS());
			sh.PlayMacro(new eMacroTapeLoad);
		}
		return ok;
	}
#else
	virtual bool Open(struct stream *stream) const
	{
		bool ok = sh.speccy->Device<eTape>()->Open(Type(), stream);
		if(ok && op_auto_play_image)
		{
			sh.OnAction(A_RESET);
			sh.speccy->Devices().Get<eRom>()->SelectPage(sh.speccy->Devices().Get<eRom>()->ROM_SOS());
			sh.PlayMacro(new eMacroTapeLoad);
		}
		return ok;
	}
#endif
	virtual const char* Type() const { return "tap"; }
} ft_tap;
#ifndef NO_USE_CSW
static struct eFileTypeCSW : public eFileTypeTAP
{
	virtual const char* Type() const { return "csw"; }
} ft_csw;
#endif
#ifndef NO_USE_TZX
static struct eFileTypeTZX : public eFileTypeTAP
{
	virtual const char* Type() const { return "tzx"; }
} ft_tzx;
#endif
#endif

}
//namespace xPlatform

// see platform-specific files for main() function
