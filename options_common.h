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

#ifndef __OPTIONS_COMMON_H__
#define __OPTIONS_COMMON_H__

#pragma once

namespace xPlatform
{

enum eJoystick { J_FIRST, J_KEMPSTON = J_FIRST, J_CURSOR, J_QAOP, J_SINCLAIR2, J_LAST };
enum eSound { S_FIRST, S_BEEPER = S_FIRST, S_AY, S_TAPE, S_LAST };
enum eVolume { V_FIRST, V_MUTE = V_FIRST, V_10, V_20, V_30, V_40, V_50, V_60, V_70, V_80, V_90, V_100, V_LAST };
enum eDrive { D_FIRST, D_A = D_FIRST, D_B, D_C, D_D, D_LAST };
enum eVsyncRate { VR_FIRST, VR_5060 = VR_FIRST , VR_50, VR_60, VR_FREE, VR_WRONG, VR_LAST };

const char* OpLastFolder();
const char* OpLastFile();
void OpLastFile(const char* path);

#ifndef USE_MU_SIMPLIFICATIONS
bool OpQuit();
void OpQuit(bool v);
#endif

#ifndef NO_USE_FDD
eDrive OpDrive();
void OpDrive(eDrive d);
#endif

#ifndef NO_USE_KEMPSTON
#ifndef USE_MU
eJoystick OpJoystick();
void OpJoystick(eJoystick v);
dword OpJoyKeyFlags();
#endif
#endif

#ifndef USE_MU
eVolume OpVolume();
void OpVolume(eVolume v);
#else
eVolume OpSoundVolume();
void OpSoundVolume(eVolume v);
eVolume OpTapeVolume();
void OpTapeVolume(eVolume v);
#ifndef NO_USE_TAPE
bool OpTapeRefresh();
#endif
#endif

eSound OpSound();
void OpSound(eSound s);

void OpVsyncRate(eVsyncRate vr);
}
//namespace xPlatform

#endif//__OPTIONS_COMMON_H__
