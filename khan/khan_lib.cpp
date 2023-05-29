/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "khan_lib.h"
#include "pico/scanvideo.h"
#if PICO_ON_DEVICE
#include "z80khan.h"
#endif
#include "khan_init.h"

#if PICO_ON_DEVICE
#include "sdl_keys.h"
#else
#include <unistd.h>
#endif

struct menu_state kms;

#include "../platform/platform.h"
#include "../platform/io.h"
#include "../options_common.h"
#include "../speccy.h"
#include "../devices/memory.h"
#include "../devices/ula.h"
#include "../z80/z80.h"
#include "../file_type.h"
#include "../tools/options.h"
#include "games/games.h"

#ifndef NO_USE_AY
#include "../devices/sound/ay.h"
#endif

#include "khan_lib.h"

bool no_wait_for_vblank;


extern void spoono();
//eMemory memory;
//eDevices devices;
//xZ80::eZ80* cpu;

int frame_tacts;

void khan_reset() {
}

void khan_zx_key_event(uint8_t key, uint8_t flags) {
    //printf("onkey %02x %02x\n", key, flags);
    xPlatform::Handler()->OnKey(key, flags);
}

bool last_button_state;
static __maybe_in_ram void khan_button_state(bool down) {
    if (down && !last_button_state) {
        xPlatform::Handler()->OnKey('e', xPlatform::KF_DOWN);
    } else if (!down && last_button_state) {
        xPlatform::Handler()->OnKey('e', 0);
    }
    last_button_state = down;
}

//#define REGENERATE_COLORS
#ifdef REGENERATE_COLORS
static uint32_t colors[512];
static uint32_t dark_colors[512];
#else
// todo const - need space for hobbit atm
static const uint32_t colors[512] = {
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0x0020c820, 0x00200039, 0x0020c839, 0x00200660, 0x0020ce60, 0x00200679, 0x0020ce79,
        0xc8200020, 0xc820c820, 0xc8200039, 0xc820c839, 0xc8200660, 0xc820ce60, 0xc8200679, 0xc820ce79,
        0x00390020, 0x0039c820, 0x00390039, 0x0039c839, 0x00390660, 0x0039ce60, 0x00390679, 0x0039ce79,
        0xc8390020, 0xc839c820, 0xc8390039, 0xc839c839, 0xc8390660, 0xc839ce60, 0xc8390679, 0xc839ce79,
        0x06600020, 0x0660c820, 0x06600039, 0x0660c839, 0x06600660, 0x0660ce60, 0x06600679, 0x0660ce79,
        0xce600020, 0xce60c820, 0xce600039, 0xce60c839, 0xce600660, 0xce60ce60, 0xce600679, 0xce60ce79,
        0x06790020, 0x0679c820, 0x06790039, 0x0679c839, 0x06790660, 0x0679ce60, 0x06790679, 0x0679ce79,
        0xce790020, 0xce79c820, 0xce790039, 0xce79c839, 0xce790660, 0xce79ce60, 0xce790679, 0xce79ce79,
        0x00200020, 0x0020f820, 0x0020003f, 0x0020f83f, 0x002007e0, 0x0020ffe0, 0x002007ff, 0x0020ffff,
        0xf8200020, 0xf820f820, 0xf820003f, 0xf820f83f, 0xf82007e0, 0xf820ffe0, 0xf82007ff, 0xf820ffff,
        0x003f0020, 0x003ff820, 0x003f003f, 0x003ff83f, 0x003f07e0, 0x003fffe0, 0x003f07ff, 0x003fffff,
        0xf83f0020, 0xf83ff820, 0xf83f003f, 0xf83ff83f, 0xf83f07e0, 0xf83fffe0, 0xf83f07ff, 0xf83fffff,
        0x07e00020, 0x07e0f820, 0x07e0003f, 0x07e0f83f, 0x07e007e0, 0x07e0ffe0, 0x07e007ff, 0x07e0ffff,
        0xffe00020, 0xffe0f820, 0xffe0003f, 0xffe0f83f, 0xffe007e0, 0xffe0ffe0, 0xffe007ff, 0xffe0ffff,
        0x07ff0020, 0x07fff820, 0x07ff003f, 0x07fff83f, 0x07ff07e0, 0x07ffffe0, 0x07ff07ff, 0x07ffffff,
        0xffff0020, 0xfffff820, 0xffff003f, 0xfffff83f, 0xffff07e0, 0xffffffe0, 0xffff07ff, 0xffffffff,
};
static uint32_t dark_colors[512] = {
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0xc8200020, 0x00390020, 0xc8390020, 0x06600020, 0xce600020, 0x06790020, 0xce790020,
        0x0020c820, 0xc820c820, 0x0039c820, 0xc839c820, 0x0660c820, 0xce60c820, 0x0679c820, 0xce79c820,
        0x00200039, 0xc8200039, 0x00390039, 0xc8390039, 0x06600039, 0xce600039, 0x06790039, 0xce790039,
        0x0020c839, 0xc820c839, 0x0039c839, 0xc839c839, 0x0660c839, 0xce60c839, 0x0679c839, 0xce79c839,
        0x00200660, 0xc8200660, 0x00390660, 0xc8390660, 0x06600660, 0xce600660, 0x06790660, 0xce790660,
        0x0020ce60, 0xc820ce60, 0x0039ce60, 0xc839ce60, 0x0660ce60, 0xce60ce60, 0x0679ce60, 0xce79ce60,
        0x00200679, 0xc8200679, 0x00390679, 0xc8390679, 0x06600679, 0xce600679, 0x06790679, 0xce790679,
        0x0020ce79, 0xc820ce79, 0x0039ce79, 0xc839ce79, 0x0660ce79, 0xce60ce79, 0x0679ce79, 0xce79ce79,
        0x00200020, 0xf8200020, 0x003f0020, 0xf83f0020, 0x07e00020, 0xffe00020, 0x07ff0020, 0xffff0020,
        0x0020f820, 0xf820f820, 0x003ff820, 0xf83ff820, 0x07e0f820, 0xffe0f820, 0x07fff820, 0xfffff820,
        0x0020003f, 0xf820003f, 0x003f003f, 0xf83f003f, 0x07e0003f, 0xffe0003f, 0x07ff003f, 0xffff003f,
        0x0020f83f, 0xf820f83f, 0x003ff83f, 0xf83ff83f, 0x07e0f83f, 0xffe0f83f, 0x07fff83f, 0xfffff83f,
        0x002007e0, 0xf82007e0, 0x003f07e0, 0xf83f07e0, 0x07e007e0, 0xffe007e0, 0x07ff07e0, 0xffff07e0,
        0x0020ffe0, 0xf820ffe0, 0x003fffe0, 0xf83fffe0, 0x07e0ffe0, 0xffe0ffe0, 0x07ffffe0, 0xffffffe0,
        0x002007ff, 0xf82007ff, 0x003f07ff, 0xf83f07ff, 0x07e007ff, 0xffe007ff, 0x07ff07ff, 0xffff07ff,
        0x0020ffff, 0xf820ffff, 0x003fffff, 0xf83fffff, 0x07e0ffff, 0xffe0ffff, 0x07ffffff, 0xffffffff,
        0x00200020, 0x0020c820, 0x00200039, 0x0020c839, 0x00200660, 0x0020ce60, 0x00200679, 0x0020ce79,
        0xc8200020, 0xc820c820, 0xc8200039, 0xc820c839, 0xc8200660, 0xc820ce60, 0xc8200679, 0xc820ce79,
        0x00390020, 0x0039c820, 0x00390039, 0x0039c839, 0x00390660, 0x0039ce60, 0x00390679, 0x0039ce79,
        0xc8390020, 0xc839c820, 0xc8390039, 0xc839c839, 0xc8390660, 0xc839ce60, 0xc8390679, 0xc839ce79,
        0x06600020, 0x0660c820, 0x06600039, 0x0660c839, 0x06600660, 0x0660ce60, 0x06600679, 0x0660ce79,
        0xce600020, 0xce60c820, 0xce600039, 0xce60c839, 0xce600660, 0xce60ce60, 0xce600679, 0xce60ce79,
        0x06790020, 0x0679c820, 0x06790039, 0x0679c839, 0x06790660, 0x0679ce60, 0x06790679, 0x0679ce79,
        0xce790020, 0xce79c820, 0xce790039, 0xce79c839, 0xce790660, 0xce79ce60, 0xce790679, 0xce79ce79,
        0x00200020, 0x0020f820, 0x0020003f, 0x0020f83f, 0x002007e0, 0x0020ffe0, 0x002007ff, 0x0020ffff,
        0xf8200020, 0xf820f820, 0xf820003f, 0xf820f83f, 0xf82007e0, 0xf820ffe0, 0xf82007ff, 0xf820ffff,
        0x003f0020, 0x003ff820, 0x003f003f, 0x003ff83f, 0x003f07e0, 0x003fffe0, 0x003f07ff, 0x003fffff,
        0xf83f0020, 0xf83ff820, 0xf83f003f, 0xf83ff83f, 0xf83f07e0, 0xf83fffe0, 0xf83f07ff, 0xf83fffff,
        0x07e00020, 0x07e0f820, 0x07e0003f, 0x07e0f83f, 0x07e007e0, 0x07e0ffe0, 0x07e007ff, 0x07e0ffff,
        0xffe00020, 0xffe0f820, 0xffe0003f, 0xffe0f83f, 0xffe007e0, 0xffe0ffe0, 0xffe007ff, 0xffe0ffff,
        0x07ff0020, 0x07fff820, 0x07ff003f, 0x07fff83f, 0x07ff07e0, 0x07ffffe0, 0x07ff07ff, 0x07ffffff,
        0xffff0020, 0xfffff820, 0xffff003f, 0xfffff83f, 0xffff07e0, 0xffffffe0, 0xffff07ff, 0xffffffff,
};
#endif
static int color_toggle = 0;

#define SPECTRUM_BRIGHTNESS(c) (200 + (((c)&8)?55:0))
#define SPECTRUM_TO_RGB(c) MAKE_RGB(((c)&2) ? SPECTRUM_BRIGHTNESS(c) : 0, ((c)&4) ? SPECTRUM_BRIGHTNESS(c) : 0, ((c)&1) ? SPECTRUM_BRIGHTNESS(c) : 0)

static struct eOptionImage : public xOptions::eOptionIntWithPending
{
    eOptionImage() { value = -1; }
    virtual const char* Name() const override {
#ifndef USE_MU
        return "image";
#else
        return "Image";
#endif
    }

    virtual int Order() const override { return 54; }

    const char *Value() const override {
        int v = *this;
        if (v >= 0 && v<(int)embedded_game_count) {
            return embedded_games[v].name;
        }
        return "none";
    }

    virtual void Complete(bool accept) override
    {
        assert(is_change_pending);
        if (accept && pending_value != value) {
            SetNow(pending_value);
        }
        is_change_pending = false;
    }

    void SetNow(const int &v) override
    {
        int old_value = value;
        eOptionIntWithPending::SetNow(v);
        if (value != old_value && value >= -1 && value<=(int)embedded_game_count) {
            if (value == -1) {
                xPlatform::Handler()->OnAction(xPlatform::A_RESET);
            } else {
                const embedded_game_t *g = embedded_games + value;
#ifndef NO_USE_FAST_TAPE
                xOptions::eOptionBool::Find("Fast tape")->Set((g->flags & GF_SLOW_TAPE) == 0);
#endif
                no_wait_for_vblank = (g->flags & GF_NO_WAIT_VBLANK) != 0;
                xPlatform::Handler()->OnOpenFileCompressed(g->ext, g->data_z, g->data_z_size, g->data_size);
            }
        }
    }

    void Change(bool next) override
    {
        eOptionInt::Change(-1, embedded_game_count, next);
    }
} op_image;

int khan_init()
{
    mutex_init(&kms.mutex);
#if !PICO_ON_DEVICE
    static char buf[1024];
    if (!getcwd(buf, sizeof(buf)))
        return false;
    strcat(buf, "/");
    xIo::SetResourcePath(buf);
#endif
#ifndef NO_USE_128K
    xOptions::eOptionBool::Find("48K mode")->Set(false);
#endif
    xPlatform::Handler()->OnInit();
//    xPlatform::Handler()->Speccy()->CPU()->set_breakpoint(0xe7); //0x6EB1); //0x6D31);
    //
    op_image.SetNow(embedded_game_default); // actually select image

#if !PICO_ON_DEVICE

//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/tests/z80tests.tap"));

//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/z80full.tap"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/Manic_Miner_1983_Software_Projects.z80"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/Jet Set Willy (R D Foord Software).tzx"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/knightlore.sna"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/g+g.sna"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/JETSET.TAP"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/3D_Pacman_1983_Freddy_Kristiansen.z80"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/Donkey_Kong_1986_Ocean.z80"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/Hobbit_The_v1.2_1982_Melbourne_House_a.z80"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/Star_Wars_1987_Domark.z80"));
//    xPlatform::Handler()->OnOpenFile(xIo::ResourcePath("res/The Sentinel.tzx"));
    platform_key_down = khan_key_down;
    platform_key_up = khan_key_up;
#endif
#ifdef REGENERATE_COLORS
    // make colortab: zx-attr -> pc-attr
    for(int a = 0; a < 0x100; a++) {
        byte ink = a & 7;
        byte paper = (a >> 3) & 7;
        byte bright = (a >> 6) & 1;
        byte flash = (a >> 7) & 1;
        if (ink)
            ink |= bright << 3;        // no bright for 0th color
        if (paper)
            paper |= bright << 3;    // no bright for 0th color
//        if (ink == paper) {
//            ink = paper = 0;
//        }
        colors[a] = (SPECTRUM_TO_RGB(ink) << 16) | SPECTRUM_TO_RGB(paper);
        if (flash) {
            colors[a + 256] = SPECTRUM_TO_RGB(ink) | (SPECTRUM_TO_RGB(paper) << 16);
        } else {
            colors[a + 256] = colors[a];
        }
        dark_colors[a] = colors[a];
        dark_colors[a + 256] = colors[a+256];
    }
    printf("static uint32_t colors[512] = {\n");
    for(int i=0;i<0x200;i+=8) {
        printf("\t");
        for(int j=0;j<8;j++) {
            printf("0x%08x, ", colors[i+j]);
        }
        printf("\n");
    }
    printf("};\n");
    printf("static uint32_t dark_colors[512] = {\n");
    for(int i=0;i<0x200;i+=8) {
        printf("\t");
        for(int j=0;j<8;j++) {
            printf("0x%08x, ", colors[i+j]);
        }
        printf("\n");
    }
    printf("};\n");
#endif
#if !PICO_ON_DEVICE
    spoono();
#endif
    return 0;
}


void khan_done() {
    xPlatform::Handler()->OnDone();
}

static int frame;
static int frame2;
static bool do_pulldown56;

static bool option_next, option_complete;
static xOptions::eOptionB* option_needed;

void __maybe_in_ram khan_loop()
{
#if defined(USE_BANKED_MEMORY_ACCESS) && PICO_ON_DEVICE
    xPlatform::Handler()->Speccy()->CPU()->InitMemoryBanks();
#endif
    while (true) {
        if (!xPlatform::Handler()->IsVideoPaused()) {
            if (do_pulldown56 && ++frame2 >= 6) {
                frame2 = 0;
            } else {
                khan_cb_begin_frame();
                xPlatform::Handler()->OnLoop();
                if (++frame >= 15) {
                    frame = 0;
                    color_toggle ^= 256;
                }
                khan_cb_end_frame();
            }
        }
        if (!no_wait_for_vblank) scanvideo_wait_for_vblank();
        khan_button_state(khan_cb_is_button_down());
        xOptions::eOptionB *option;
        bool next;

        mutex_enter_blocking(&kms.mutex);
        option = option_needed;
        next = option_next;
        option_needed = NULL;
        if (xPlatform::OpTapeRefresh()) {
            kms.do_fill_menu = true;
        }
        if (option) {
            if (option_complete)
            {
                // switch images is slow... we block this thread anyway, but releasing the mutex allows the video thread to still update the screen!!
                mutex_exit(&kms.mutex);
                option->Complete();
                mutex_enter_blocking(&kms.mutex);
                kms.do_hide_menu = true;
                kms.flashing = false;
            } else {
                const char *v = option->Value();
                option->Change(next);
                // bit of a hack to detect unchangable values
                if (v == option->Value()) {
                    kms.error_level = MENU_ERROR_LEVEL_MAX;
                }
                if (option->IsAction())
                {
                    kms.do_hide_menu = true;
                    kms.flashing = false;
                }
                else
                {
                    kms.do_fill_menu = true;
                }
            }
        }
        mutex_exit(&kms.mutex);
    }
}

int __maybe_in_ram khan_go(int video_mode_hz) {
    do_pulldown56 = video_mode_hz == 60;
    if (do_pulldown56) {
        OpVsyncRate(xPlatform::VR_5060);
    } else if (video_mode_hz == 50) {
        OpVsyncRate(xPlatform::VR_50);
    } else {
        OpVsyncRate(xPlatform::VR_WRONG);
    }
    khan_loop();
    khan_done();
    return 0;
}

bool __maybe_in_ram khan_get_scanline_info(int l, uint16_t* border, const uint8_t ** attr, const uint8_t ** pixels, const uint32_t **attr_colors, const uint32_t **dark_attr_colors) {
    eUla *ula = xPlatform::Handler()->Speccy()->Devices().Get<eUla>();
    uint8_t bborder;
    ula->GetLineInfo(l, bborder, *attr, *pixels);
    *border = colors[bborder&0xf]>>16;
    *attr_colors = colors + color_toggle;
    if (kms.opacity) {
        *dark_attr_colors = dark_colors + color_toggle;
    }
    return true;
}

using xOptions::eOptionB;

void __maybe_in_ram khan_fill_main_menu() {
    kms.num_lines = 0;
    for(eOptionB* o = eOptionB::First(); o; o = o->Next()) {
        kms.lines[kms.num_lines].left_text.text = o->Name();
        kms.lines[kms.num_lines].right_text.text = o->Value();
        kms.lines[kms.num_lines].left_text.width = 0;
        kms.lines[kms.num_lines].right_text.width = 0;
        kms.num_lines++;
        if (kms.num_lines == MENU_MAX_LINES) break;
    }
    for(int i=kms.num_lines;i<MENU_MAX_LINES;i++) {
        kms.lines[i].left_text.text = NULL;
        kms.lines[i].right_text.text = NULL;
        kms.lines[i].left_text.width = 0;
        kms.lines[i].right_text.width = 0;
    }
}

void __maybe_in_ram khan_idle_blanking() {
    if (kms.error_level > 0) {
        kms.error_level--;
    }
    if (kms.flashing) {
        kms.flash_pos = (kms.flash_pos + 1);
        if (kms.flash_pos == MENU_FLASH_LENGTH) kms.flash_pos = 0;
    }
    int next_opacity = kms.opacity;
    if (kms.appearing) {
        if (next_opacity < MENU_OPACITY_MAX) {
            next_opacity = kms.opacity+2;
        } else {
            kms.appearing = false;
        }
    } else if (kms.disappearing) {
        if (next_opacity > 0) {
            next_opacity = kms.opacity-2;
        } else {
            kms.disappearing = false;
        }
    }
    if (next_opacity != kms.opacity) {
        int l = 32 - kms.opacity;
        for(int i=0;i<512;i++) {
            uint32_t cmask = 0x001f001f;
            uint32_t r = (colors[i] >> PICO_SCANVIDEO_PIXEL_RSHIFT) & cmask;
            uint32_t g = (colors[i] >> PICO_SCANVIDEO_PIXEL_GSHIFT) & cmask;
            uint32_t b = (colors[i] >> PICO_SCANVIDEO_PIXEL_BSHIFT) & cmask;
            uint32_t br = 4;
            uint32_t bg = 2;
            uint32_t bb = 7;
            r = (((r * l)>>5) + ((br*kms.opacity)>>5)) & cmask;
            g = (((g * l)>>5) + ((bg*kms.opacity)>>5)) & cmask;
            b = (((b * l)>>5) + ((bb*kms.opacity)>>5)) & cmask;
            dark_colors[i] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
        }
        kms.opacity = next_opacity;
    }
}

void __maybe_in_ram khan_defer_option(eOptionB *option, bool next, bool complete = false) {
    mutex_enter_blocking(&kms.mutex);
    option_needed = option;
    option_next = next;
    option_complete = complete;
    mutex_exit(&kms.mutex);
}

static inline void nav_error() {
    kms.error_level = MENU_ERROR_LEVEL_MAX;
}

// called to update the menu
bool __maybe_in_ram khan_menu_selection_change(enum menu_key key)
{
    int i = kms.selected_line;
    eOptionB *o;
    for (o = eOptionB::First(); o && i; o = o->Next(), i--) {}
    if (o) {
        switch (key) {
            case MK_NONE: {
                // bit of a hack.. just a safe place to do this once per frame
                kms.flashing = o->IsChangePending();
                return false;
            }
            case MK_UP:
                if (!o->IsChangePending())
                {
                    kms.selected_line = (kms.selected_line + kms.num_lines - 1) % kms.num_lines;
                } else nav_error();
                return true;
            case MK_DOWN:
                if (!o->IsChangePending())
                {
                    kms.selected_line = (kms.selected_line + 1) % kms.num_lines;
                } else nav_error();
                return true;
            case MK_LEFT:
                if (!o->IsAction()) {
                    // tell cpu thread to run the action
                    khan_defer_option(o, false);
                } else nav_error();
                return true;
            case MK_RIGHT:
                if (!o->IsAction()) {
                    khan_defer_option(o, true);
                } else nav_error();
                return true;
            case MK_ENTER:
                if (o->IsAction() || o->IsChangePending()) {
                    khan_defer_option(o, true, o->IsChangePending());
                }
                return true;
            case MK_ESCAPE:
                if (o->IsChangePending()) {
                    o->Complete(false);
                    kms.do_fill_menu = true;
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}

#ifndef NO_USE_KEMPSTON
#include "../devices/input/kempston_joy.h"

void khan_set_joystick_state(uint8_t state) {
    xPlatform::Handler()->Speccy()->Devices().Get<eKempstonJoy>()->setState(state);
}
#endif
namespace xIo {

bool MkDir(const char* path) {
    assert(false);
    return false;
}

}

#if !PICO_ON_DEVICE
#include <SDL.h>
#endif

namespace xPlatform {
#if false && !PICO_ON_DEVICE
    static bool PreProcessKey(SDL_Event& e)
    {
        if(e.key.keysym.mod)
            return false;
        if(e.type != SDL_KEYUP)
            return false;
        switch(e.key.keysym.sym)
        {
            case SDL_SCANCODE_F2:
            {
                using namespace xOptions;
                eOptionB* o = eOptionB::Find("save state");
                SAFE_CALL(o)->Change();
            }
                return true;
            case SDL_SCANCODE_F3:
            {
                using namespace xOptions;
                eOptionB* o = eOptionB::Find("load state");
                SAFE_CALL(o)->Change();
            }
                return true;
            case SDL_SCANCODE_F5:
#ifndef NO_USE_TAPE
                Handler()->OnAction(A_TAPE_TOGGLE);
#endif
                return true;
            case SDL_SCANCODE_F7:
            {
                using namespace xOptions;
                eOptionB* o = eOptionB::Find("pause");
                SAFE_CALL(o)->Change();
            }
                return true;
            case SDL_SCANCODE_F12:
                Handler()->OnAction(A_RESET);
                return true;
            default:
                return false;
        }
    }
#endif

    static byte TranslateScancode(SDL_Scancode _scancode, dword& _flags)
    {
        switch(_scancode)
        {
            case SDL_SCANCODE_GRAVE:return 'm';
            case SDL_SCANCODE_LSHIFT:	return 'c';
            case SDL_SCANCODE_RSHIFT:	return 'c';
            case SDL_SCANCODE_LALT:		return 's';
            case SDL_SCANCODE_RALT:		return 's';
            case SDL_SCANCODE_RETURN:	return 'e';
            case SDL_SCANCODE_BACKSPACE:
                _flags |= KF_SHIFT;
                return '0';
            case SDL_SCANCODE_APOSTROPHE:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'P';
                }
                else
                    return '7';
            case SDL_SCANCODE_COMMA:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'R';
                }
                else
                    return 'N';
            case SDL_SCANCODE_PERIOD:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'T';
                }
                else
                    return 'M';
            case SDL_SCANCODE_SEMICOLON:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'Z';
                }
                else
                    return 'O';
            case SDL_SCANCODE_SLASH:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'C';
                }
                else
                    return 'V';
            case SDL_SCANCODE_MINUS:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return '0';
                }
                else
                    return 'J';
            case SDL_SCANCODE_EQUALS:
                _flags |= KF_ALT;
                if(_flags&KF_SHIFT)
                {
                    _flags &= ~KF_SHIFT;
                    return 'K';
                }
                else
                    return 'L';
            case SDL_SCANCODE_TAB:
                _flags |= KF_ALT;
                _flags |= KF_SHIFT;
                return 0;
            case SDL_SCANCODE_LEFT:		return 'l';
            case SDL_SCANCODE_RIGHT:	return 'r';
            case SDL_SCANCODE_UP:		return 'u';
            case SDL_SCANCODE_DOWN:		return 'd';
            case SDL_SCANCODE_INSERT:
            case SDL_SCANCODE_RCTRL:
            case SDL_SCANCODE_LCTRL:	return 'f';
            case SDL_SCANCODE_0:        return '0';
            default:
                break;
        }
        if(_scancode >= SDL_SCANCODE_1 && _scancode <= SDL_SCANCODE_9)
            return _scancode + '1' - SDL_SCANCODE_1;
        if(_scancode >= SDL_SCANCODE_A && _scancode <= SDL_SCANCODE_Z)
            return 'A' + _scancode - SDL_SCANCODE_A;
        if(_scancode == SDL_SCANCODE_SPACE)
            return ' ';
        return 0;
    }
}

bool cursor_mode = true;

void update_cursor_mode(int scan) {
    cursor_mode = scan >= SDL_SCANCODE_RIGHT && scan <= SDL_SCANCODE_UP;
}

void khan_key_down(int scan, int sym, int mod) {
    if (scan == SDL_SCANCODE_ESCAPE) {
        khan_menu_key(MK_ESCAPE);
        return;
    }
    update_cursor_mode(scan);
    if (!kms.opacity || kms.disappearing)
    {
        dword flags = xPlatform::KF_DOWN;
        if (mod & KMOD_ALT)
            flags |= xPlatform::KF_ALT;
        if (mod & KMOD_SHIFT)
            flags |= xPlatform::KF_SHIFT;
        if (cursor_mode)
            flags |= xPlatform::KF_CURSOR;
        byte key = xPlatform::TranslateScancode((SDL_Scancode)scan, flags);
        khan_zx_key_event(key, flags);
    } else {
        switch (scan) {
            case SDL_SCANCODE_LEFT:
                khan_menu_key(MK_LEFT);
                return;
            case SDL_SCANCODE_RIGHT:
                khan_menu_key(MK_RIGHT);
                return;
            case SDL_SCANCODE_DOWN:
                khan_menu_key(MK_DOWN);
                return;
            case SDL_SCANCODE_UP:
                khan_menu_key(MK_UP);
                return;
            case SDL_SCANCODE_RETURN:
                khan_menu_key(MK_ENTER);
                return;
        }
    }
}

void khan_key_up(int scan, int sym, int mod) {
    if (sym == SDL_SCANCODE_ESCAPE) {
        return;
    }
    dword flags = 0;//xPlatform::KF_DOWN;
    if(mod&KMOD_ALT)
        flags |= xPlatform::KF_ALT;
    if(mod&KMOD_SHIFT)
        flags |= xPlatform::KF_SHIFT;
    update_cursor_mode(scan);
    if (cursor_mode)
        flags |= xPlatform::KF_CURSOR;
    byte key = xPlatform::TranslateScancode((SDL_Scancode)scan, flags);
    khan_zx_key_event(key, flags);
}

#ifndef NO_USE_AY
int ay_sample_rate() {
    return SNDR_DEFAULT_SAMPLE_RATE;
}
#endif