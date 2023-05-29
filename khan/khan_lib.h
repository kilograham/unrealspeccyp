/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef KHAN_LIB_H
#define KHAN_LIB_H
#include "pico.h"
#include "pico/sync.h"

//#if PICO_ON_DEVICE
//#define __maybe_in_ram __attribute__((section(".time_critical")))
//#else
#define __maybe_in_ram
//#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int khan_init();
extern int khan_go(int video_mode_hz);

void khan_key_down(int scan, int sym, int mod);
void khan_key_up(int scan, int sym, int mod);

bool khan_get_scanline_info(int l, uint16_t *border, const uint8_t **attr, const uint8_t **pixels, const uint32_t **attr_colors, const uint32_t **dark_attr_colors);

// these calls are into khan from the rendering thread

// called from the rendering thread after the last visible line is rendered... a good time to do some per frame stuff
extern void khan_idle_blanking();
extern void khan_fill_main_menu();
extern void khan_zx_key_event(uint8_t key, uint8_t flags);
enum menu_key
{
    MK_NONE,
    MK_ESCAPE,
    MK_UP,
    MK_DOWN,
    MK_LEFT,
    MK_RIGHT,
    MK_ENTER
};
extern void khan_menu_key(enum menu_key key);
extern void khan_hide_menu();

// callbacks from khan on the cpu thread
extern bool khan_cb_is_button_down();
extern void khan_cb_begin_frame();
extern void khan_cb_end_frame();
// returns true if the key was consumed
extern bool khan_menu_selection_change(enum menu_key key);
#ifdef USE_KHAN_GPIO
extern uint8_t khan_gpio_read(uint reg);
extern void khan_gpio_write(uint reg, uint8_t value);
#endif

#ifndef NO_USE_KEMPSTON
#define KEMPSTON_R 0x01
#define KEMPSTON_L 0x02
#define KEMPSTON_D 0x04
#define KEMPSTON_U 0x08
#define KEMPSTON_F 0x10
extern void khan_set_joystick_state(uint8_t state);
#endif

extern void khan_beeper_reset();
extern void khan_beeper_begin_frame(uint32_t t, int32_t frame_num);
// t is within frame
extern void khan_beeper_level_change(uint32_t t, uint8_t level);
extern void khan_beeper_end_frame(uint32_t t);

struct text_element
{
    const char *text;
    int width;
};


extern const uint8_t atlantis_glyph_bitmap[];

extern const uint8_t atlantis_glyph_widths[];

#define MENU_GLYPH_MIN 32
#define MENU_GLYPH_COUNT 95
#define MENU_GLYPH_MAX (MENU_GLYPH_MIN + MENU_GLYPH_COUNT - 1)
#define MENU_GLYPH_HEIGHT 9
#define MENU_GLYPH_Y_OFFSET 2
#define MENU_GLYPH_ADVANCE 1

// so we're offset a bit
#define MENU_WIDTH_IN_BLOCKS 17
#define MENU_LEFT_WIDTH_IN_BLOCKS 10
#define MENU_RIGHT_WIDTH_IN_BLOCKS (MENU_WIDTH_IN_BLOCKS - MENU_LEFT_WIDTH_IN_BLOCKS)

#define MENU_LINE_HEIGHT 12
#define MENU_LINE_BORDER_WIDTH 4
#define MENU_LINE_TEXT_INDENT 6
#ifndef NO_USE_128K
#define MENU_MAX_LINES 11
#else
#define MENU_MAX_LINES 9
#endif

// must be <32
#define MENU_AREA_WIDTH_IN_BLOCKS 24
#define MENU_AREA_OFFSET_Y 4
#define MENU_AREA_BORDER_HEIGHT 2
// how opaque the bg is
//#define MENU_OPACITY_MAX 24
//#define MENU_OPACITY_OFFSET 0
#define MENU_OPACITY_MAX 20
#define MENU_OPACITY_OFFSET 3
// went the fg kicks in
#define MENU_OPACITY_THRESHOLD 10

#define MENU_AREA_WIDTH (MENU_AREA_WIDTH_IN_BLOCKS * 8)
#define MENU_WIDTH (MENU_WIDTH_IN_BLOCKS * 8)
#define MENU_BORDER_WIDTH_IN_BLOCKS ((MENU_AREA_WIDTH-MENU_WIDTH)/2)

// note this is assumed to be 16
#define MENU_ERROR_LEVEL_MAX 16
#define MENU_FLASH_LENGTH 32

#define menu_glypth_bitmap atlantis_glyph_bitmap
#define menu_glyph_widths atlantis_glyph_widths

struct menu_state
{
    struct mutex mutex;
    struct
    {
        struct text_element left_text;
        struct text_element right_text;
    } lines[MENU_MAX_LINES];
    int16_t opacity;
    bool appearing;
    bool disappearing;
    bool flashing;

    int8_t num_lines;
    int8_t selected_line;
    int8_t selection_top_pixel;
    int8_t flash_pos;
    int8_t error_level;

    // protected by mutex
    bool do_hide_menu;
    bool do_fill_menu;
};

extern struct menu_state kms;

#ifndef NO_USE_AY
int ay_sample_rate();
#endif

#ifdef __cplusplus
}
#endif

#endif
