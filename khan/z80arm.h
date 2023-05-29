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

#ifndef	__Z80ARM_H__
#define	__Z80ARM_H__

#include "../std.h"
#include "z80khan.h"

#pragma once

class eMemory;
class eRom;
class eUla;
class eDevices;

namespace xZ80
{
    enum eFlags
    {
        CF = 0x01,
        NF = 0x02,
        PV = 0x04,
        F3 = 0x08,
        HF = 0x10,
        F5 = 0x20,
        ZF = 0x40,
        SF = 0x80
    };

//*****************************************************************************
//	eZ80
//-----------------------------------------------------------------------------
    void z80a_step_hook_func();

    class eZ80
    {
    public:
        eZ80(eMemory* m, eDevices* d, dword frame_tacts);
#ifdef USE_BANKED_MEMORY_ACCESS
        void InitMemoryBanks(); // must be called on correct core
#endif
        void Reset();
        void Update(int int_len, int* nmi_pending);
#ifndef NO_USE_REPLAY
        void Replay(int fetches);
#endif

        dword FrameTacts() const { return frame_tacts; }
        dword T() const {
            assert(!z80a_resting_state.active); // until we find we need it
            return z80a_resting_state.t;
        }

#ifndef USE_HACKED_DEVICE_ABSTRACTION
        class eHandlerIo
        {
        public:
            virtual byte Z80_IoRead(word port, int tact) = 0;
        };
        void HandlerIo(eHandlerIo* h) { handler.io = h; }
        eHandlerIo* HandlerIo() const { return handler.io; }
#endif

        class eHandlerStep
        {
        public:
            virtual void Z80_Step(eZ80* z80) = 0;
        };
#ifndef NO_USE_FAST_TAPE
        void HandlerStep(eHandlerStep* h) {
            if (h == NULL) {
                z80a_resting_state.step_hook_func = 0;
            } else {
                assert(!z80a_resting_state.active);
                z80a_resting_state.step_hook_func = &z80a_step_hook_func;
            }
            handler.step = h;
        }
        eHandlerStep* HandlerStep() const {
            return handler.step;
        }
#endif

        //protected:
//        void Int();
        void Nmi();
//        void Step();
//        void StepF();

//        byte IoRead(word port) const;
//        void IoWrite(word port, byte v);
//        byte Read(word addr) const;
//        byte ReadInc(int& addr) const;
//        int Read2(word addr) const;
//        int Read2Inc(int& addr) const;
//        void Write(word addr, byte v);
//        void Write2(word addr, int v);

//        typedef void (eZ80::*CALLFUNC)();
//        typedef byte (eZ80::*CALLFUNCI)(byte);

#ifndef NDEBUG
        void set_breakpoint(int bp_addr);
#endif
    protected:
        eMemory*	memory;
        eRom*		rom;
        eUla*		ula;
        eDevices*	devices;

        struct eHandler {
            eHandler() {
#ifndef USE_HACKED_DEVICE_ABSTRACTION
                io = NULL;
#endif
#ifndef NO_USE_FAST_TAPE
                step = NULL;
#endif
            }
#ifndef USE_HACKED_DEVICE_ABSTRACTION
            eHandlerIo*	io;
#endif
#ifndef NO_USE_FAST_TAPE
            eHandlerStep* step;
#endif
        } handler;

        int		eipos;
        const int		frame_tacts; 	// t-states per frame
#ifndef NO_USE_REPLAY
        int		fetches;		// .rzx replay fetches
#endif

        inline struct _z80a_resting_state *get_caller_regs() const {
            assert(!z80a_resting_state.active);
            return &z80a_resting_state;
        }
        inline word get_caller_pc() const {
            return get_caller_regs()->pc;
        }
        inline void set_caller_pc(word v) const {
            get_caller_regs()->pc = v;
        }
        inline byte get_caller_a() const { return get_caller_regs()->a; }
        inline void set_caller_a(byte v) { get_caller_regs()->a = v; }
        inline void set_caller_flag(byte flags) { get_caller_regs()->f |= flags; }
        inline byte get_caller_b() const { return get_caller_regs()->b; }
        inline void set_caller_b(byte v) { get_caller_regs()->b = v; }
        inline byte get_caller_c() const { return get_caller_regs()->c; }
        inline void set_caller_bc(word v) {
            get_caller_regs()->bc = v;
        }
        inline word get_caller_de() const { return get_caller_regs()->de; }
        inline void set_caller_de(word v) {
            get_caller_regs()->de = v;
        }
        inline word get_caller_ix() const { return get_caller_regs()->ix; }
        inline void set_caller_ix(word v) {
            get_caller_regs()->ix = v;
        }
        inline byte get_caller_l() const { return get_caller_regs()->l; }
        inline void set_caller_l(byte v) { get_caller_regs()->l = v; }
        inline void set_caller_h(byte v) { get_caller_regs()->h = v; }
        inline void delta_caller_t(int delta) { get_caller_regs()->t += delta; }
    };

}//namespace xZ80

#endif//__Z80_H__
