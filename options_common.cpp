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

#include "platform/platform.h"
#include "platform/io.h"
#include "tools/options.h"
#ifdef USE_UI
#include "ui/ui.h"
#endif
#include "options_common.h"
#include "file_type.h"

namespace xPlatform
{

#ifndef NO_USE_SAVE
static struct eOptionStoreSlot : public xOptions::eOptionInt
{
	virtual const char* Name() const { return "save slot"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(0, 10, next);
	}
	virtual int Order() const { return 7; }
} op_save_slot;

struct eOptionSave : public xOptions::eOptionBool
{
	eOptionSave() { storeable = false; }
	virtual const char*	Value() const { return NULL; }
	bool PrepareName()
	{
		strcpy(name, OpLastFile());
		ext[0] = '\0';
		int l = strlen(name);
		if(!l || name[l - 1] == '/' || name[l - 1] == '\\')
			return false;
		for(const eFileType* t = eFileType::First(); t; t = t->Next())
		{
			char contain_path[xIo::MAX_PATH_LEN];
			char contain_name[xIo::MAX_PATH_LEN];
			if(t->Contain(name, contain_path, contain_name))
			{
				strcpy(name, contain_path);
				strcat(name, "#/");
				strcat(name, contain_name);
				l = strlen(name);
			}
		}
		char* e = name + l;
		while(e > name && *e != '.' && *e != '\\' && *e != '/')
			--e;
		if(*e != '.')
			return false;
		strcpy(ext, e);
		*e = '\0';
		while(e > name && *e != '@' && *e != '\\' && *e != '/')
			--e;
		if(*e == '@')
			*e = '\0';
		if(op_save_slot)
		{
			strcat(name, "@");
			strcat(name, op_save_slot.Value());
		}
		return true;
	}
	const char* FileName()
	{
		if(!PrepareName())
			return NULL;
		strcat(name, ext);
		return name;
	}
	const char* SnapshotName()
	{
		if(!PrepareName())
			return NULL;
		strcat(name, ".sna");
		return name;
	}
	char name[xIo::MAX_PATH_LEN];
	char ext[xIo::MAX_PATH_LEN];
};

static struct eOptionSaveFile : public eOptionSave
{
	virtual const char* Name() const { return "save file"; }
	virtual void Change(bool next = true)
	{
		const char* name = FileName();
		if(name)
			Set(Handler()->OnSaveFile(name));
		else
			Set(false);
	}
	virtual int Order() const { return 8; }
} op_save_file;
#endif

#ifndef NO_USE_SAVE
static struct eOptionSaveState : public eOptionSave
{
	virtual const char* Name() const { return "save state"; }
	virtual void Change(bool next = true)
	{
		const char* name = SnapshotName();
		if(name)
			Set(Handler()->OnSaveFile(name));
		else
			Set(false);
	}
	virtual int Order() const { return 5; }
} op_save_state;

static struct eOptionLoadState : public eOptionSave
{
	virtual const char* Name() const { return "load state"; }
	virtual void Change(bool next = true)
	{
		const char* name = SnapshotName();
		if(name)
			Set(Handler()->OnOpenFile(name));
		else
			Set(false);
	}
	virtual int Order() const { return 6; }
} op_load_state;
#endif

#ifndef NO_USE_TAPE
static struct eOptionTape : public xOptions::eOptionIntWithPending
{
	eOptionTape() { storeable = false; }
	virtual const char* Name() const override {
#ifndef USE_MU
		return "tape";
#else
		return "Tape";
#endif
	}
	enum {
		V_NONE = 0,
		V_STOPPED = 1,
		V_PLAYING = 2,
		V_STOP = 3,
		V_PLAY = 4,
		V_REWIND = 5
	};
	const char** Values() const override
	{
		static const char* values[] = { "none", "stopped", "playing", "stop", "play", "rewind", NULL };
		return values;
	}
	static int translate(eActionResult result) {
		switch(result)
		{
			case AR_TAPE_STOPPED:
				return V_STOPPED;
			case AR_TAPE_STARTED:
				return V_PLAYING;
			default:
				return V_NONE;
		}
	}

	void Change(bool next = true) override {
		int v = eOptionTape::translate(Handler()->OnAction(A_TAPE_QUERY));
		if (v == V_NONE) {
			SetNow(V_NONE);
		} else {
			int cur;
			if (*this == V_PLAY || *this == V_STOP) {
				cur = 1;
			} else if (*this == V_REWIND) {
				cur = 2;
			} else {
				cur = 0;
			}
			cur += (next?1:2);
			if (cur >= 3) cur -= 3;
			if (cur == 0) {
				Set(value); // set to the current value (i.e no longer pending)
			} else if (cur == 1) {
				Set(v == V_PLAYING ? V_STOP : V_PLAY);
			} else {
				Set(V_REWIND);
			}
		}
	}

	void Complete(bool accept) override
	{
		if (accept && is_change_pending) {
			SetNow(translate(Handler()->OnAction(A_TAPE_TOGGLE)));
		}
		is_change_pending = false;
	}

	int Order() const override { return 40; }

	bool Refresh() {
		int v = eOptionTape::translate(Handler()->OnAction(A_TAPE_QUERY));
		if (v != value) {
			// underlying state change compared to current (actual) value
			SetNow(v);
			return true;
		}
		return false;
	}
} op_tape;

bool OpTapeRefresh() {
	return op_tape.Refresh();
}
#endif

static struct eOptionPause : public xOptions::eOptionBoolYesNo
{
	eOptionPause() { storeable = false; }
	virtual const char* Name() const {
#ifndef USE_MU
		return "pause";
#else
		return "Paused";
#endif
	}
	virtual void Change(bool next = true)
	{
		eOptionBool::Change();
		Handler()->VideoPaused(*this);
	}
	virtual int Order() const { return 70; }
} op_pause;

#if 0 //ndef NO_USE_AY
static struct eOptionSound : public xOptions::eOptionInt
{
	eOptionSound() {
		Set(S_AY);
	}
	virtual const char* Name() const { return "Sound"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "beeper", "ay", "tape", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(S_FIRST, S_LAST, next);
	}
	virtual int Order() const { return 20; }
} op_sound;
#endif

#ifndef NO_USE_SOUND
#ifndef USE_MU
static struct eOptionSound : public xOptions::eOptionInt
{
	eOptionSound() { Set(S_AY); }
	virtual const char* Name() const { return "sound"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "beeper", "ay", "tape", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(S_FIRST, S_LAST, next);
	}
	virtual int Order() const { return 20; }
} op_sound;
eVolume	OpVolume() { return (eVolume)(int)op_sound; }
void OpVolume(eVolume v) { op_sound.Set(v); }

#else
struct eOptionVolume : public xOptions::eOptionInt
{
	eOptionVolume() { Set(V_70); }
	virtual const char** Values() const
	{
		static const char* values[] = { "mute", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(V_FIRST, V_LAST, next);
	}
};

static struct eOptionSoundVolume : public eOptionVolume
{
	eOptionSoundVolume() {
#ifndef NO_USE_AY
		Set(V_50);
#else
		Set(V_30);
#endif
	}
	virtual const char* Name() const {
		return "Speaker volume";
	}
	virtual int Order() const { return 30; }
} op_sound_volume;

static struct eOptionTapeVolume : public eOptionVolume
{
	eOptionTapeVolume() { Set(V_30); }
	virtual const char* Name() const { return "Tape volume"; }
	virtual int Order() const { return 32; }
} op_tape_volume;
#endif

eVolume	OpSoundVolume() { return (eVolume)(int)op_sound_volume; }
void OpSoundVolume(eVolume v) { op_sound_volume.Set(v); }
eVolume	OpTapeVolume() { return (eVolume)(int)op_tape_volume; }
void OpTapeVolume(eVolume v) { op_tape_volume.Set(v); }
#endif


#if 0 // ifndef NO_USE_AY
eSound	OpSound() { return (eSound)(int)op_sound; }
void OpSound(eSound s) { op_sound.Set(s); }
#endif

#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
static struct eOptionJoy : public xOptions::eOptionInt
{
	eOptionJoy() { Set(J_KEMPSTON); }
	virtual const char* Name() const { return "joystick"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "kempston", "cursor", "qaop", "sinclair2", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(J_FIRST, J_LAST, next);
	}
	virtual int Order() const { return 10; }
} op_joy;
#endif
#endif
#ifndef NO_USE_FDD
static struct eOptionDrive : public xOptions::eOptionInt
{
	eOptionDrive() { storeable = false; Set(D_A); }
	virtual const char* Name() const { return "drive"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "A", "B", "C", "D", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(D_FIRST, D_LAST, next);
	}
	virtual int Order() const { return 60; }
} op_drive;
#endif

#ifdef USE_MU
	static struct eOptionVSyncRate : public xOptions::eOptionInt
	{
		eOptionVSyncRate() { storeable = false; }
		virtual const char* Name() const { return "VSync rate"; }
		virtual const char** Values() const
		{
			static const char* values[] = { "50/60", "50", "60", "free", "wrong", NULL };
			return values;
		}
		virtual int Order() const { return 67; }
	} op_vsync;
	void OpVsyncRate(eVsyncRate vr) { op_vsync.Set(vr); }
#endif

static struct eOptionReset : public xOptions::eOptionB
{
	eOptionReset() {
#ifdef USE_MU
		is_action = true;
#endif
		storeable = false;
	}
	virtual const char* Name() const {
#ifndef USE_MU
		return "reset";
#else
		return "Reset";
#endif
	}
	virtual void Change(bool next = true) { Handler()->OnAction(A_RESET); }
	virtual int Order() const { return 80; }
} op_reset;

#ifndef USE_MU_SIMPLIFICATIONS
static struct eOptionQuit : public xOptions::eOptionBool
{
	eOptionQuit() { storeable = false; }
	virtual const char* Name() const { return "quit"; }
	virtual int Order() const { return 100; }
	virtual const char** Values() const { return NULL; }
} op_quit;
#endif

#ifndef USE_MU_SIMPLIFICATIONS
static struct eOptionLastFile : public xOptions::eOptionString
{
	eOptionLastFile() { customizable = false; }
	virtual const char* Name() const { return "last file"; }
} op_last_file;

const char* OpLastFile() { return op_last_file; }
const char* OpLastFolder()
{
	static char lf[xIo::MAX_PATH_LEN];
	strcpy(lf, OpLastFile());
	int l = strlen(lf);
	if(!l || lf[l - 1] == '\\' || lf[l - 1] == '/')
		return lf;
	char parent[xIo::MAX_PATH_LEN];
	xIo::GetPathParent(parent, lf);
	strcpy(lf, parent);
	strcat(lf, "/");
	return lf;
}
void OpLastFile(const char* name) { op_last_file.Set(name); }
#endif

#ifndef USE_MU_SIMPLIFICATIONS
bool OpQuit() { return op_quit; }
void OpQuit(bool v) { op_quit.Set(v); }
#endif

#ifndef NO_USE_FDD
eDrive OpDrive() { return (eDrive)(int)op_drive; }
void OpDrive(eDrive d) { op_drive.Set(d); }
#endif

#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
eJoystick OpJoystick() { return (eJoystick)(int)op_joy; }
void OpJoystick(eJoystick v) { op_joy.Set(v); }
dword OpJoyKeyFlags()
{
	switch(op_joy)
	{
	case J_KEMPSTON:	return KF_KEMPSTON;
	case J_CURSOR:		return KF_CURSOR;
	case J_QAOP:		return KF_QAOP;
	case J_SINCLAIR2:	return KF_SINCLAIR2;
	}
	return KF_QAOP;
}
#endif
#endif
}
//namespace xPlatform
