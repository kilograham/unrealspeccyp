/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/param.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/multicore.h"
#include "khan_lib.h"
#if PICO_ON_DEVICE
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "sdl_keys.h"
#endif
#include "khan_init.h"
#include "khan.pio.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"

#if defined(USE_USB_KEYBOARD) || defined(USE_USB_MOUSE)
#define USE_USB 1
#define USB_ON_CORE1 1
#include "tusb.h"
#include "hardware/irq.h"
#endif

#include "pico/binary_info.h"
#ifdef USE_USB_KEYBOARD
bi_decl(bi_program_feature("USB keyboard support"));
#endif
#ifdef USE_USB_MOUSE
bi_decl(bi_program_feature("USB mouse support"));
#endif
#ifdef USE_SDL_EVENT_FORWARDER_INPUT
bi_decl(bi_program_feature("SDL event forwarder support"));
#endif
#ifdef USE_UART_KEYBOARD
bi_decl(bi_program_feature("Raw UART keyboard support (limited)"));
#endif
//#define HACK_NON_CIRCULAR
#define ENABLE_MENU_OVERLAY
#if defined(ENABLE_MENU_OVERLAY) && PICO_SCANVIDEO_PLANE_COUNT > 1
#define ENABLE_MENU_PLANE_RENDERING
#endif

#ifdef PICO_SCANVIDEO_48MHZ
#define CLOCK_MHZ 48000000
#else
#define CLOCK_MHZ 50000000
#endif
// todo yikes IRQs take so long that we run out of time with audio
// rendering doesn't take that much time, so use core 0 which has interrupts on it, leaving core1 100% time for z80
#define RENDER_ON_CORE0
//#define RENDER_ON_CORE1

// other IRQs are there... save time by having it on core1
//#define BEEPER_IRQ_ON_RENDER_THREAD

CU_REGISTER_DEBUG_PINS(khan_timing, beeper, menu)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(khan_timing)
//CU_SELECT_DEBUG_PINS(beeper)
//CU_SELECT_DEBUG_PINS(menu)

//#ifdef NDEBUG
#define DISABLE_BEEPER_ASSERTIONS
//#endif
#ifndef DISABLE_BEEPER_ASSERTIONS
#define beeper_assert(x) assert(x)
#else
#define beeper_assert(x) ((void)0)
#endif

#if defined(USE_UART_KEYBOARD) || defined(USE_SDL_EVENT_FORWARDER_INPUT)
void khan_check_uart();
#endif
#ifdef USE_USB
void khan_check_usb();
#endif
#ifndef NO_USE_BEEPER
void khan_beeper_setup();
void khan_beeper_enable();
#endif

typedef bool (*render_scanline_func)(struct scanvideo_scanline_buffer *dest, int core);
bool render_scanline_khan(struct scanvideo_scanline_buffer *dest, int core);

#define the_video_mode video_mode_khan
//#define the_video_mode video_mode_khan_tft

//#define the_video_mode video_mode_khan2
//#define the_video_mode video_mode_khan3
//#define the_video_mode video_mode_khan_small_border_50
//#define video_mode video_mode_320x240_60

render_scanline_func render_scanline = render_scanline_khan;

int vspeed
= 1*1;
int hspeed = 1*-1;

// to make sure only one core updates the state when the frame number changes
// todo note we should actually make sure here that the other core isn't still rendering (i.e. all must arrive before either can proceed - a la barrier)
static struct mutex frame_logic_mutex;

#ifdef ENABLE_MENU_OVERLAY
static uint32_t menu_text_color;
static uint32_t menu_highlight_bg_color;
static uint32_t menu_highlight_fg_color;
static uint32_t menu_highlight_fg_color2;
#endif

static void khan_update_menu_visuals();

//void __isr isr_siob_proc0() {
//	gpio_put(24, 1);
//}

#ifndef NO_USE_AY
#if KHAN128_I2S
#include "pico/audio_i2s.h"
#else
#include "pico/audio_pwm.h"
#endif

struct audio_buffer_pool *producer_pool;

bool ay_init() {
    const struct audio_format *output_format;

#if KHAN128_I2S
#if !PICO_NO_BINARY_INFO
    bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DATA", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCLK",
                                PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCLK"));
#endif
    struct audio_i2s_config ay_config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 4,
            .pio_sm = 0,
    };
#else
#ifdef NO_USE_BEEPER
    bi_decl(bi_1pin_with_name(PICO_AUDIO_PWM_L_PIN, "AY sound (PWM)"));
#else
    bi_decl(bi_1pin_with_name(PICO_AUDIO_PWM_R_PIN, "AY sound (PWM)"));
#endif
    static struct audio_pwm_channel_config ay_config = {
#ifdef NO_USE_BEEPER
            .core.base_pin = PICO_AUDIO_PWM_L_PIN,
#else
            .core.base_pin = PICO_AUDIO_PWM_R_PIN,
#endif
            .core.dma_channel = 4,
            .core.pio_sm = 0,
    };
#endif

    static struct audio_format producer_audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = 0,
            .channel_count = 1,
    };
    producer_audio_format.sample_freq = ay_sample_rate();

    static struct audio_buffer_format producer_format = {
            .format = &producer_audio_format,
            .sample_stride = 2
    };

    bool __unused ok = false;

    producer_pool = audio_new_producer_pool(&producer_format, 3, PICO_AUDIO_BUFFER_SAMPLE_LENGTH); // we need 441-443 so this seems reasonable

#if KHAN128_I2S
    output_format = audio_i2s_setup(&producer_audio_format, &ay_config);
#else
    output_format = audio_pwm_setup(&producer_audio_format, -1, &ay_config);
#endif
    if (!output_format) {
        printf("MuAudio: Unable to open audio device.\n");
        return false;
    } else
    {
#if KHAN128_I2S
        audio_i2s_connect(producer_pool);
#else
        audio_pwm_set_correction_mode(fixed_dither);
        audio_pwm_default_connect(producer_pool, false);
#endif
#if defined(SPEEDUP) && PICO_ON_DEVICE && !KHAN128_I2S
        // hack to slow down PWM
        pio_sm_set_clkdiv_int_frac(pio1, 0, SPEEDUP, 0);
#endif
#if KHAN128_I2S
        audio_i2s_set_enabled(true);
#else
        audio_pwm_set_enabled(true);
#endif
        return true;
    }
}
#endif

void main_loop() {

//    gpio_init(PIN_DBG1);
//    gpio_dir_out_mask(1 << PIN_DBG1); // steal debug pin 1 for this core
//    gpio_init(PIN_DBG1+1);
//    gpio_init(PIN_DBG1+2);
//    gpio_dir_out_mask(6 << PIN_DBG1); // steal debug pin 2,3 for this core
    int hz = (the_video_mode.default_timing->clock_freq + (the_video_mode.default_timing->h_total * the_video_mode.default_timing->v_total) / 2) /
            (the_video_mode.default_timing->h_total * the_video_mode.default_timing->v_total);
#ifdef NO_USE_128K
    puts("\n===============\nWelcome to Khan\n");
#else
    puts("\n===================\nWelcome to Khan128\nnote: currently beeper sound is disabled in favor of AY (wip)\n");
#endif
    printf("Hz %d\n", hz);
#ifndef NO_USE_BEEPER
#ifndef BEEPER_IRQ_ON_RENDER_THREAD
    khan_beeper_enable();
#endif
#endif
#ifndef NO_USE_AY
    // todo on its own we need to hook up ISR
    ay_init();
#endif
#ifdef USE_USB
#if USE_USB && USB_ON_CORE1
    tusb_init();
    irq_set_priority(USBCTRL_IRQ, 0xc0);
#endif
#endif
    khan_go(hz);
}

int render_loop() {
//    gpio_init(PIN_DBG1);
//    gpio_dir_out_mask(1 << PIN_DBG1); // steal debug pin 1 for this core
//    gpio_init(PIN_DBG1+1);
//    gpio_init(PIN_DBG1+2);
//    gpio_dir_out_mask(6 << PIN_DBG1); // steal debug pin 2,3 for this core

#ifndef NO_USE_BEEPER
#ifdef BEEPER_IRQ_ON_RENDER_THREAD
    khan_beeper_enable();
#endif
#endif
	static uint32_t last_frame_num = 0;
	int core_num = get_core_num();
	assert(core_num >=0 && core_num < 2);
//	printf("khan rendering on core %d\r\n", core_num);
	while (true) {
		struct scanvideo_scanline_buffer *scanvideo_scanline_buffer = scanvideo_begin_scanline_generation(true);
//		if (scanvideo_scanline_buffer->data_used) {
//            // validate the previous scanline to make sure noone corrupted it
//            validate_scanline(scanvideo_scanline_buffer->data, scanvideo_scanline_buffer->data_used, video_mode.width, video_mode.width);
//        }
		// do any frame related logic
		mutex_enter_blocking(&frame_logic_mutex);
		uint32_t frame_num = scanvideo_frame_number(scanvideo_scanline_buffer->scanline_id);
		// note that with multiple cores we may have got here not for the first scanline, however one of the cores will do this logic first before either does the actual generation
		if (frame_num != last_frame_num) {
		    DEBUG_PINS_SET(khan_timing, 4);
            khan_menu_selection_change(MK_NONE);
#if defined(USE_UART_KEYBOARD) || defined(USE_SDL_EVENT_FORWARDER_INPUT)
            khan_check_uart();
#endif
#ifdef USE_USB
            khan_check_usb();
#endif
            khan_update_menu_visuals();
            khan_idle_blanking();
            DEBUG_PINS_CLR(khan_timing, 4);
			// this could should be during vblank as we try to create the next line
			// todo should we ignore if we aren't attempting the next line
			last_frame_num = frame_num;
		}
		mutex_exit(&frame_logic_mutex);
        DEBUG_PINS_SET(khan_timing, 2);
        render_scanline(scanvideo_scanline_buffer, core_num);
        DEBUG_PINS_CLR(khan_timing, 2);
#ifdef ENABLE_VIDEO_PLANE3
        assert(false);
#endif
        // release the scanline into the wild
		scanvideo_end_scanline_generation(scanvideo_scanline_buffer);
	}
}

#ifndef USE_CONST_CMD_LOOKUP
uint32_t cmd_lookup[256];
#else
const uint32_t cmd_lookup[256] = {
        0xb0000000, 0xf0000000, 0xb5000000, 0xf5000000, 0xb0500000, 0xf0500000, 0xb5500000, 0xf5500000,
        0xb0050000, 0xf0050000, 0xb5050000, 0xf5050000, 0xb0550000, 0xf0550000, 0xb5550000, 0xf5550000,
        0xb0005000, 0xf0005000, 0xb5005000, 0xf5005000, 0xb0505000, 0xf0505000, 0xb5505000, 0xf5505000,
        0xb0055000, 0xf0055000, 0xb5055000, 0xf5055000, 0xb0555000, 0xf0555000, 0xb5555000, 0xf5555000,
        0xb0000500, 0xf0000500, 0xb5000500, 0xf5000500, 0xb0500500, 0xf0500500, 0xb5500500, 0xf5500500,
        0xb0050500, 0xf0050500, 0xb5050500, 0xf5050500, 0xb0550500, 0xf0550500, 0xb5550500, 0xf5550500,
        0xb0005500, 0xf0005500, 0xb5005500, 0xf5005500, 0xb0505500, 0xf0505500, 0xb5505500, 0xf5505500,
        0xb0055500, 0xf0055500, 0xb5055500, 0xf5055500, 0xb0555500, 0xf0555500, 0xb5555500, 0xf5555500,
        0xb0000050, 0xf0000050, 0xb5000050, 0xf5000050, 0xb0500050, 0xf0500050, 0xb5500050, 0xf5500050,
        0xb0050050, 0xf0050050, 0xb5050050, 0xf5050050, 0xb0550050, 0xf0550050, 0xb5550050, 0xf5550050,
        0xb0005050, 0xf0005050, 0xb5005050, 0xf5005050, 0xb0505050, 0xf0505050, 0xb5505050, 0xf5505050,
        0xb0055050, 0xf0055050, 0xb5055050, 0xf5055050, 0xb0555050, 0xf0555050, 0xb5555050, 0xf5555050,
        0xb0000550, 0xf0000550, 0xb5000550, 0xf5000550, 0xb0500550, 0xf0500550, 0xb5500550, 0xf5500550,
        0xb0050550, 0xf0050550, 0xb5050550, 0xf5050550, 0xb0550550, 0xf0550550, 0xb5550550, 0xf5550550,
        0xb0005550, 0xf0005550, 0xb5005550, 0xf5005550, 0xb0505550, 0xf0505550, 0xb5505550, 0xf5505550,
        0xb0055550, 0xf0055550, 0xb5055550, 0xf5055550, 0xb0555550, 0xf0555550, 0xb5555550, 0xf5555550,
        0xb0000005, 0xf0000005, 0xb5000005, 0xf5000005, 0xb0500005, 0xf0500005, 0xb5500005, 0xf5500005,
        0xb0050005, 0xf0050005, 0xb5050005, 0xf5050005, 0xb0550005, 0xf0550005, 0xb5550005, 0xf5550005,
        0xb0005005, 0xf0005005, 0xb5005005, 0xf5005005, 0xb0505005, 0xf0505005, 0xb5505005, 0xf5505005,
        0xb0055005, 0xf0055005, 0xb5055005, 0xf5055005, 0xb0555005, 0xf0555005, 0xb5555005, 0xf5555005,
        0xb0000505, 0xf0000505, 0xb5000505, 0xf5000505, 0xb0500505, 0xf0500505, 0xb5500505, 0xf5500505,
        0xb0050505, 0xf0050505, 0xb5050505, 0xf5050505, 0xb0550505, 0xf0550505, 0xb5550505, 0xf5550505,
        0xb0005505, 0xf0005505, 0xb5005505, 0xf5005505, 0xb0505505, 0xf0505505, 0xb5505505, 0xf5505505,
        0xb0055505, 0xf0055505, 0xb5055505, 0xf5055505, 0xb0555505, 0xf0555505, 0xb5555505, 0xf5555505,
        0xb0000055, 0xf0000055, 0xb5000055, 0xf5000055, 0xb0500055, 0xf0500055, 0xb5500055, 0xf5500055,
        0xb0050055, 0xf0050055, 0xb5050055, 0xf5050055, 0xb0550055, 0xf0550055, 0xb5550055, 0xf5550055,
        0xb0005055, 0xf0005055, 0xb5005055, 0xf5005055, 0xb0505055, 0xf0505055, 0xb5505055, 0xf5505055,
        0xb0055055, 0xf0055055, 0xb5055055, 0xf5055055, 0xb0555055, 0xf0555055, 0xb5555055, 0xf5555055,
        0xb0000555, 0xf0000555, 0xb5000555, 0xf5000555, 0xb0500555, 0xf0500555, 0xb5500555, 0xf5500555,
        0xb0050555, 0xf0050555, 0xb5050555, 0xf5050555, 0xb0550555, 0xf0550555, 0xb5550555, 0xf5550555,
        0xb0005555, 0xf0005555, 0xb5005555, 0xf5005555, 0xb0505555, 0xf0505555, 0xb5505555, 0xf5505555,
        0xb0055555, 0xf0055555, 0xb5055555, 0xf5055555, 0xb0555555, 0xf0555555, 0xb5555555, 0xf5555555,
};
#endif

int video_main(void) {
	mutex_init(&frame_logic_mutex);
	// set 18-22 to RIO for debugging
//	for (int i = PIN_DBG1; i < 22; ++i)
//        gpio_init(i);

//    gpio_init(24);
//    gpio_init(22);
//	// just from this core
//	gpio_dir_out_mask(0x01380000);
//	gpio_dir_in_mask( 0x00400000);
//
//	//gpio_funcsel(22, 0);
//
//	// debug pin
//	gpio_put(24, 0);

	// go for launch (debug pin)
#ifndef NO_USE_BEEPER
    khan_beeper_setup();
#endif

#if !PICO_ON_DEVICE
    void simulate_video_pio_video_khan(const uint32_t *dma_data, uint32_t dma_data_size,
                                       uint16_t *pixel_buffer, int32_t max_pixels, int32_t expected_width, bool overlay);
    scanvideo_set_simulate_scanvideo_pio_fn(VIDEO_KHAN_PROGRAM_NAME, simulate_video_pio_video_khan);
#endif
	//video_setup_with_timing(&the_video_mode, &video_timing_648x480_60_alt1);
    scanvideo_setup(&the_video_mode);

#ifndef USE_CONST_CMD_LOOKUP
    for(int i=0;i<256;i++) {
        uint32_t c = 0;
        for(int j=0; j<8;j++) {
            uint b;
            if (i & (1<<(7-j))) {
                b = j==7 ? video_khan_offset_ink_end_of_block : video_khan_offset_ink;
            } else {
                b = j==7 ? video_khan_offset_paper_end_of_block : video_khan_offset_paper;
            }
            c |= (b << (j*4));
        }
        cmd_lookup[i] = c;
    }
#endif
//    printf("static uint32_t cmd_lookup[256] = {\n");
//    for(int i=0;i<0x100;i+=8) {
//        printf("\t");
//        for(int j=0;j<8;j++) {
//            printf("0x%08x, ", cmd_lookup[i+j]);
//        }
//        printf("\n");
//    }
//    printf("};\n");

    khan_init();
    // start video after the init which can take about 70ms when decompressing from flash
    scanvideo_timing_enable(true);

	// button here so we have video running (albeit with red bar)
#if PICO_ON_DEVICE
#if 0
    printf("Press button to start\r\n");
    // todo NOTE THAT ON STARTUP RIGHT NOW WITH RESET ISSUES ON FPGA, THIS CURRENTLY DOES NOT STOP!!! if you make last_input static, then it never releases instead :-(
    uint8_t last_input = 0;
    while (true) {
        uint8_t new_input = gpio_get(input_pin0);
        if (last_input && !new_input) {
            break;
        }
        last_input = new_input;
        yield();
    }
#endif
#endif
//    gpio_put(24, 1);
//    gpio_funcsel(input_pin0, GPIO_FUNC_PROC);


#if USE_USB && !USB_ON_CORE1
    tusb_init();
    irq_set_priority(USBCTRL_IRQ, 0xc0);
#endif

#ifdef RENDER_ON_CORE1
	// todo it seems that maybe we get into trouble if this is too slow... first time after FPGA
	// is power up after a while, we get consistant dump everything red error scanlines
	platform_launch_core1(render_loop);
	main_loop();
#else
    multicore_launch_core1(main_loop);
	render_loop();
#endif
	__builtin_unreachable();
}

bool khan_cb_is_button_down() {
    // todo default button
//#if !PICO_ON_DEVICE
//    return gpio_get(input_pin0);
//#else
    return false;
//#endif
}

void __maybe_in_ram measure_text(struct text_element *element, int max) {
    if (!element->width)
    {
        element->width = 0;
        if (element->text)
        {
            const char *p = element->text;
            uint8_t c;
            while ((c = *p++))
            {
                c -= MENU_GLYPH_MIN;
                if (c < MENU_GLYPH_MAX)
                {
                    element->width += menu_glyph_widths[c] + MENU_GLYPH_ADVANCE;
                }
            }
            if (element->width > max)
                element->width = max;
        }
    }
}

void khan_hide_menu() {
    kms.appearing = false;
    if (kms.opacity)
    {
        kms.disappearing = true;
    }
}

void khan_menu_key(enum menu_key key) {
    if (key == MK_ESCAPE) {
        if (kms.appearing || kms.opacity) {
            if (!khan_menu_selection_change(MK_ESCAPE)) {
                khan_hide_menu();
            }
        } else if (kms.disappearing || !kms.opacity) {
            kms.disappearing = false;
            kms.appearing = true;
            khan_fill_main_menu();
        }
    } else {
        if (kms.opacity && !kms.disappearing) {
            switch (key) {
                case MK_UP:
                case MK_DOWN:
                case MK_LEFT:
                case MK_RIGHT:
                case MK_ENTER:
                    khan_menu_selection_change(key);
                    break;
                default:
                    break;
            }
        }
    }
}

//#define MAKE_RGB(r,g,b) (0x8000u|(((r)>>3))|(((g)>>3)<<5)|(((b)>>3)<<10))
//#define MAKE_BGR(b,g,r) MAKE_RGB(r,g,b)

#ifdef ENABLE_MENU_OVERLAY
// doesn't need to be this big really, given the big gap in the middle, but this is simplest (for one color menu anyway)
// note we don't store the empty lines between glyphs
static uint8_t menu_bitmap[MENU_WIDTH_IN_BLOCKS * MENU_GLYPH_HEIGHT * MENU_MAX_LINES];
#endif

int render_font_line(const struct text_element *element, uint8_t *out, const uint8_t *bitmaps, int pos, int bits);

uint32_t mix_frac16(uint32_t a, uint32_t b, int level) {
    int rr = (PICO_SCANVIDEO_R5_FROM_PIXEL(a) * (16 - level) + PICO_SCANVIDEO_R5_FROM_PIXEL(b) * level) / 16;
    int gg = (PICO_SCANVIDEO_G5_FROM_PIXEL(a) * (16 - level) + PICO_SCANVIDEO_G5_FROM_PIXEL(b) * level) / 16;
    int bb = (PICO_SCANVIDEO_B5_FROM_PIXEL(a) * (16 - level) + PICO_SCANVIDEO_B5_FROM_PIXEL(b) * level) / 16;
    return PICO_SCANVIDEO_ALPHA_MASK | PICO_SCANVIDEO_PIXEL_FROM_RGB5(rr, gg, bb);
}

// called once per frame
void khan_update_menu_visuals() {
    bool hide;
    bool fill;
    mutex_enter_blocking(&kms.mutex);
    hide = kms.do_hide_menu; kms.do_hide_menu = false;
    fill = kms.do_fill_menu; kms.do_fill_menu = false;
    mutex_exit(&kms.mutex);
    if (hide) khan_hide_menu();
    if (fill) khan_fill_main_menu();
#ifdef ENABLE_MENU_OVERLAY
    DEBUG_PINS_SET(menu, 1);
    if (kms.opacity > MENU_OPACITY_THRESHOLD) {
        menu_text_color = PICO_SCANVIDEO_ALPHA_MASK + PICO_SCANVIDEO_PIXEL_FROM_RGB5(1, 1, 1) * (kms.opacity + MENU_OPACITY_OFFSET);
        menu_highlight_fg_color = PICO_SCANVIDEO_ALPHA_MASK + PICO_SCANVIDEO_PIXEL_FROM_RGB5(1, 1, 1) * (kms.opacity + MENU_OPACITY_OFFSET + 7);

        if (kms.error_level) {
            menu_highlight_fg_color = mix_frac16(menu_highlight_fg_color, PICO_SCANVIDEO_PIXEL_FROM_RGB5(0x1f, 0, 0), kms.error_level);
        }
        menu_highlight_fg_color2 = menu_highlight_fg_color;
        if (kms.flashing) {
            int level;
            if (kms.flash_pos < 8)
                level = kms.flash_pos * 2;
            else if ((MENU_FLASH_LENGTH - kms.flash_pos) < 8)
                level = (MENU_FLASH_LENGTH - kms.flash_pos) * 2;
            else
                level = 16;
            menu_highlight_fg_color2 = mix_frac16(PICO_SCANVIDEO_PIXEL_FROM_RGB5(0x10, 0x10, 0x10), menu_highlight_fg_color2, level);
        }
//        menu_highlight_bg_color = kms.opacity > MENU_OPACITY_OFFSET + MENU_OPACITY_MAX - 6 ? MAKE_RGB(0x40, 0x40, 0xc0) : 0;
        menu_highlight_bg_color = kms.opacity > MENU_OPACITY_OFFSET + MENU_OPACITY_MAX - 6 ? PICO_SCANVIDEO_ALPHA_MASK + PICO_SCANVIDEO_PIXEL_FROM_RGB8(48,138,208) : 0;
    } else {
        menu_text_color = menu_highlight_bg_color = menu_highlight_fg_color = 0;
    }
    static int foobar;
    foobar++;

    if (kms.opacity)
    {
        static int refresh_line = 0;
        measure_text(&kms.lines[refresh_line].left_text, MENU_LEFT_WIDTH_IN_BLOCKS * 8);
        measure_text(&kms.lines[refresh_line].right_text, MENU_RIGHT_WIDTH_IN_BLOCKS * 8);

        uint8_t *out = menu_bitmap + refresh_line * MENU_GLYPH_HEIGHT * MENU_WIDTH_IN_BLOCKS;
        const uint8_t *bitmaps = menu_glypth_bitmap;
        for (int j = 0; j < MENU_GLYPH_HEIGHT; j++)
        {
            int pos = 0;
            int bits = 0;
            if (kms.lines[refresh_line].left_text.text)
            {
                pos = render_font_line(&kms.lines[refresh_line].left_text, out, bitmaps, pos, bits);
            }
            while (pos < MENU_LEFT_WIDTH_IN_BLOCKS)
                out[pos++] = 0;
            pos = MENU_LEFT_WIDTH_IN_BLOCKS; // truncate
            if (kms.lines[refresh_line].right_text.text)
            {
                int xoff = MENU_RIGHT_WIDTH_IN_BLOCKS * 8 - kms.lines[refresh_line].right_text.width;
                int post = pos + (xoff >> 3);
                while (pos < post)
                    out[pos++] = 0;
                bits = xoff & 7;
                pos = render_font_line(&kms.lines[refresh_line].right_text, out, bitmaps, pos, bits);
            }
            while (pos < MENU_WIDTH_IN_BLOCKS)
                out[pos++] = 0;
            bitmaps++;
            out += pos;
        }
        refresh_line++;
        if (refresh_line == MENU_MAX_LINES) refresh_line = 0;
    }
    int expected_top_pixel = kms.selected_line * MENU_LINE_HEIGHT;
    int delta = expected_top_pixel - kms.selection_top_pixel;
    if (delta > 1) {
        kms.selection_top_pixel += 1 + (((delta + MENU_LINE_HEIGHT / 2)* ((1 << 16) / MENU_LINE_HEIGHT))>>16);
    } else if (delta < -1) {
        kms.selection_top_pixel -= 1 + (((MENU_LINE_HEIGHT/2 - delta) * ((1 << 16) / MENU_LINE_HEIGHT))>>16);
    } else {
        kms.selection_top_pixel = expected_top_pixel;
    }
    DEBUG_PINS_CLR(menu, 1);
#endif
}

int render_font_line(const struct text_element *element, uint8_t *out, const uint8_t *bitmaps, int pos, int bits) {
    const char *p = element->text;
    uint32_t acc = 0;
    uint8_t c;
    while ((c = *p++)) {
        c -= MENU_GLYPH_MIN;
        if (c < MENU_GLYPH_MAX) {
            int cbits = menu_glyph_widths[c] + 1;
            if (cbits <= 8) {
                acc = (acc << cbits) | ((bitmaps[c * MENU_GLYPH_HEIGHT] >> (8 - cbits)));
            } else {
                acc = (acc << cbits) | ((bitmaps[c * MENU_GLYPH_HEIGHT] << (cbits - 8)));
            }
            bits += cbits;
            if (bits >= 8) {
                bits -= 8;
                out[pos++] = acc >> bits;
                acc &= ((1 << bits) - 1);
            }
        }
    }
    if (bits) {
        out[pos++] = acc << (8 - bits);
    }
    return pos;
}

#pragma GCC push_options
#ifdef __arm__
#pragma GCC optimize("O3")
#endif

uint32_t *render_menu_line(int l, uint32_t *buf2);

bool __time_critical_func(render_scanline_khan)(struct scanvideo_scanline_buffer *dest, int core) {
    // 1 + line_num red, then white
    uint32_t *buf = dest->data;
    uint l = scanvideo_scanline_number(dest->scanline_id);
    const uint8_t *attr = 0;
    const uint8_t *pixels = 0;
    uint16_t border = 0;
    const uint32_t *attr_colors = 0;
    const uint32_t *dark_attr_colors = 0;
    int border_top = (the_video_mode.height - 192) / 2;
    khan_get_scanline_info(l - border_top, &border, &attr, &pixels, &attr_colors, &dark_attr_colors);

#if PICO_SCANVIDEO_PLANE_COUNT > 1
    uint32_t *buf2 = dest->data2;
#endif
#define MIN_COLOR_RUN 3
    if (attr) {
        int border_width = (the_video_mode.width - 256) / 2;
        if (border_width >= MIN_COLOR_RUN) {
            *buf++ = border | ((border_width - MIN_COLOR_RUN) << 16u);
            *buf++ = video_khan_offset_color_run;
        }
        l -= border_top + MENU_AREA_OFFSET_Y;
        if (!(dark_attr_colors && l >= 0 && l <= MENU_LINE_HEIGHT * MENU_MAX_LINES + MENU_AREA_BORDER_HEIGHT * 2)) {
            dark_attr_colors = attr_colors;
        }
#define regular(n) *buf++ = attr_colors[attr[n]], *buf++ = cmd_lookup[pixels[n]]
#define maybe_dark(n) *buf++ = dark_attr_colors[attr[n]], *buf++ = cmd_lookup[pixels[n]]

        regular(0);
        regular(1);
        regular(2);
        regular(3);

        maybe_dark(4);
        maybe_dark(5);
        maybe_dark(6);
        maybe_dark(7);

        maybe_dark(8);
        maybe_dark(9);
        maybe_dark(10);
        maybe_dark(11);

        maybe_dark(12);
        maybe_dark(13);
        maybe_dark(14);
        maybe_dark(15);

        maybe_dark(16);
        maybe_dark(17);
        maybe_dark(18);
        maybe_dark(19);

        maybe_dark(20);
        maybe_dark(21);
        maybe_dark(22);
        maybe_dark(23);

        maybe_dark(24);
        maybe_dark(25);
        maybe_dark(26);
        maybe_dark(27);

        regular(28);
        regular(29);
        regular(30);
        regular(31);

        if (border_width >= MIN_COLOR_RUN) {
            *buf++ = border | ((border_width - MIN_COLOR_RUN) << 16u);
            *buf++ = video_khan_offset_color_run;
        }
#ifdef ENABLE_MENU_PLANE_RENDERING
        DEBUG_PINS_SET(menu, 2);
        if (kms.opacity > MENU_OPACITY_THRESHOLD) {
            l -= MENU_AREA_BORDER_HEIGHT;
            if (l > 0 && l < MENU_LINE_HEIGHT * MENU_MAX_LINES) {
                *buf2++ = 0 | ((border_width + ((256 - MENU_AREA_WIDTH) / 2) + MENU_LINE_BORDER_WIDTH - MIN_COLOR_RUN)
                        << 16);
                *buf2++ = video_khan_offset_color_run;
                buf2 = render_menu_line(l, buf2);
            }
        }
        DEBUG_PINS_CLR(menu, 2);
#endif
    } else {
        *buf++ = border | ((the_video_mode.width - MIN_COLOR_RUN) << 16u);
        *buf++ = video_khan_offset_color_run;
    }
    *buf++ = 0x0000u | (0x0000u << 16u);
    *buf++ = video_khan_offset_end_of_line;
    dest->status = SCANLINE_OK;
    int pos = buf - dest->data;
    assert(pos <= dest->data_max);
    dest->data_used = (uint16_t) pos;
#if PICO_SCANVIDEO_PLANE_COUNT > 1
    *buf2++ = 0x0000u | (0x0000u << 16u);
    *buf2++ = video_khan_offset_end_of_line;
    pos = buf2 - dest->data2;
    assert(pos <= dest->data2_max);
    dest->data2_used = (uint16_t)pos;
#endif
//    printf("%d %d\n", dest->data_used, dest->data2_used);
    return true;
}
#pragma GCC pop_options

uint32_t *render_menu_line(int l, uint32_t *buf2) {
    int menu_line = (l * ((1 << 16) / MENU_LINE_HEIGHT))>>16;
    int menu_line_offset = l - menu_line * MENU_LINE_HEIGHT;
    uint32_t bg_color, fg_color;
    if (l >= kms.selection_top_pixel && l < kms.selection_top_pixel + MENU_LINE_HEIGHT + 1) {
        fg_color = menu_highlight_fg_color;
        bg_color = menu_highlight_bg_color;
    } else {
        fg_color = menu_text_color;
        bg_color = 0;
    }
    uint32_t combined_colors = (fg_color << 16) | bg_color;
    if (menu_line_offset >= MENU_GLYPH_Y_OFFSET && menu_line_offset < MENU_GLYPH_Y_OFFSET + MENU_GLYPH_HEIGHT) {
        *buf2++ = bg_color | ((MENU_LINE_TEXT_INDENT - MIN_COLOR_RUN) << 16);
        *buf2++ = video_khan_offset_color_run;
        uint8_t *pixels = &menu_bitmap[MENU_WIDTH_IN_BLOCKS * (menu_line * MENU_GLYPH_HEIGHT + menu_line_offset - MENU_GLYPH_Y_OFFSET)];

#define regular2(n) *buf2++ = combined_colors, *buf2++ = cmd_lookup[pixels[n]]

        static_assert(MENU_LEFT_WIDTH_IN_BLOCKS == 10, "");
        regular2(0);
        regular2(1);
        regular2(2);
        regular2(3);
        regular2(4);
        regular2(5);
        regular2(6);
        regular2(7);
        regular2(8);
        regular2(9);

        *buf2++ = bg_color | ((MENU_AREA_WIDTH - (MENU_LINE_BORDER_WIDTH + MENU_LINE_TEXT_INDENT) * 2 - MENU_WIDTH - MIN_COLOR_RUN) << 16);
        *buf2++ = video_khan_offset_color_run;

        static_assert(MENU_RIGHT_WIDTH_IN_BLOCKS == 7, "");
        if (kms.flashing && menu_line == kms.selected_line) {
            combined_colors = (menu_highlight_fg_color2 << 16) | bg_color;
        }

        regular2(10);
        regular2(11);
        regular2(12);
        regular2(13);
        regular2(14);
        regular2(15);
        regular2(16);
        *buf2++ = bg_color | ((MENU_LINE_TEXT_INDENT - MIN_COLOR_RUN) << 16);
        *buf2++ = video_khan_offset_color_run;
    } else {
        *buf2++ = bg_color | ((MENU_AREA_WIDTH - MENU_LINE_BORDER_WIDTH * 2 - MIN_COLOR_RUN) << 16);
        *buf2++ = video_khan_offset_color_run;
    }
    return buf2;
}

void go_core1(void (*execute)()) {
	multicore_launch_core1(execute);
}

#if !PICO_ON_DEVICE
void simulate_video_pio_video_khan(const uint32_t *dma_data, uint32_t dma_data_size,
                                             uint16_t *pixel_buffer, int32_t max_pixels, int32_t expected_width, bool overlay) {
    const uint16_t *it = (uint16_t *)dma_data;
    assert(!(3&(intptr_t)dma_data));
    const __unused uint16_t *const dma_data_end = (uint16_t *)(dma_data + dma_data_size);
//    const uint16_t *const pixels_end = (uint16_t *)(pixel_buffer + max_pixels);
    uint16_t *pixels = pixel_buffer;
//    bool ok = false;
    bool done = false;
    const uint16_t display_enable_bit = PICO_SCANVIDEO_ALPHA_MASK; // for now
    do {
        uint16_t a = *it++;
        uint16_t b = *it++;
        // note we align checked above
        uint32_t c = *(uint32_t *) it;
        it += 2;
        bool had_eob = false;
        for (int n = 0; n < 8 && !done && !had_eob; n++) {
            uint cmd = c & 0xf;
            c >>= 4;
            switch (cmd) {
                case video_khan_offset_paper:
                    if (!overlay || (a & display_enable_bit)) {
                        *pixels++ = a;
                    } else {
                        pixels++;
                    }
                    break;
                case video_khan_offset_end_of_line:
                    done = true;
                    break;
                case video_khan_offset_ink:
                    if (!overlay || (b & display_enable_bit)) {
                        *pixels++ = b;
                    } else {
                        pixels++;
                    }
                    break;
                case video_khan_offset_color_run:
                    if (!overlay || (a & display_enable_bit)) {
                        for (uint i = 0; i < b + 3; i++) {
                            *pixels++ = a;
                        }
                    } else {
                        pixels += b + 3;
                    }
                    had_eob = true;
                    break;
                case video_khan_offset_paper_end_of_block:
                    if (!overlay || (a & display_enable_bit)) {
                        *pixels++ = a;
                    } else {
                        pixels++;
                    }
                    had_eob = true;
                    break;
                case video_khan_offset_ink_end_of_block:
                    if (!overlay || (b & display_enable_bit)) {
                        *pixels++ = b;
                    } else {
                        pixels++;
                    }
                    had_eob = true;
                    break;
            }
        }
        assert(done || had_eob);
    } while (!done);
//    assert(ok);
    assert(it == dma_data_end);
    assert(!(3&(intptr_t)(it))); // should end on dword boundary
//    assert(!expected_width || pixels == pixel_buffer + expected_width); // with the correct number of pixels (one more because we stick a black pixel on the end)
}
#endif

#if PICO_ON_DEVICE
void __cxa_pure_virtual() {
    __breakpoint();
}

#if 0
void _fini() {
    __breakpoint();
}
#endif
#endif

#include "miniz_tinfl.h"

int main(void) {
#if PICO_SCANVIDEO_48MHZ
    int base_khz = 48000;
#else
    int base_khz = 50000;
#endif

#ifndef SPEEDUP
    set_sys_clock_khz(base_khz, true);
#else
    set_sys_clock_khz(base_khz * SPEEDUP, true);
#endif
//    gpio_put(27, 0);
	setup_default_uart();

//    size_t in_bytes = 0;//avail_in;
//    size_t out_bytes = 0;//avail_out;
//    tinfl_init(&inflator);
//
//    mz_uint8 *s_outbuf = NULL, *next_out = NULL, *next_in = NULL;
//    bool infile_remaining = false;
//    tinfl_decompress(&inflator, (const mz_uint8 *)next_in, &in_bytes, s_outbuf, (mz_uint8 *)next_out, &out_bytes, (infile_remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0) | TINFL_FLAG_PARSE_ZLIB_HEADER);


    return video_main();
}

void khan_cb_begin_frame() {
    DEBUG_PINS_SET(khan_timing, 1);
}

void khan_cb_end_frame() {
    DEBUG_PINS_CLR(khan_timing, 1);
}

#if PICO_ON_DEVICE
enum eKeyFlags {
    KF_DOWN = 0x01,
    KF_SHIFT = 0x02,
    KF_CTRL = 0x04,
    KF_ALT = 0x08,

    KF_KEMPSTON = 0x10,
    KF_CURSOR = 0x20,
    KF_QAOP = 0x40,
    KF_SINCLAIR2 = 0x80,

};

#ifdef USE_UART_KEYBOARD
static uint8_t UartTranslateKey(uint8_t _key, uint8_t esc, uint8_t *_flags)
{
    // todo lookup table
        switch(_key)
        {
            case 27:
                switch (esc)
                {
                    case 'A':
                        *_flags = KF_CURSOR;
                        return 'u';
                    case 'B':
                        *_flags = KF_CURSOR;
                        return 'd';
                    case 'C':
                        *_flags = KF_CURSOR;
                        return 'r';
                    case 'D':
                        *_flags = KF_CURSOR;
                        return 'l';
                }
                break;
            case '`':
                return 'm';
//            case SDLK_LSHIFT:	return 'c';
//            case SDLK_RSHIFT:	return 'c';
//            case SDLK_LALT:		return 's';
//            case SDLK_RALT:		return 's';
            case '\r':
                return 'e';
            case 8:
                *_flags = KF_SHIFT;
                return '0';
            case '\t':
                *_flags = KF_ALT;
                *_flags = KF_SHIFT;
                return 0;
//            case SDLK_LEFT:		return 'l';
//            case SDLK_RIGHT:	return 'r';
//            case SDLK_UP:		return 'u';
//            case SDLK_DOWN:		return 'd';
//            case SDLK_INSERT:
//            case SDLK_RCTRL:
//            case SDLK_LCTRL:	return 'f';
            default:
                break;
        }
        if(_key >= '0' && _key <= '9')
            return _key;
        if(_key >= 'A' && _key <= 'Z') {
            *_flags = KF_SHIFT;
            return _key;
        }
        if(_key >= 'a' && _key <= 'z') {
            return _key + 'A' - 'a';
        }
        if(_key == ' ')
            return _key;

        const char alt_translate[] = "\"P'7<R,N>T.M:Z;O/V?C-J_0=L+K";
        for(int i = 0; i < count_of(alt_translate); i+=2) {
            if (_key == alt_translate[i]) {
                *_flags |= KF_ALT;
                return alt_translate[i+1];
            }
        }
        // todo escape codes
        printf("Unknown key %d\n", _key);
        *_flags = 0;
        return 0;
}
#endif

static bool skip_key;

static uint16_t modifier_state;

static const uint16_t mods[8] = {
        KMOD_LCTRL,
        KMOD_LSHIFT,
        KMOD_LALT,
        KMOD_LGUI,
        KMOD_RCTRL,
        KMOD_RSHIFT,
        KMOD_RALT,
        KMOD_RGUI,
};

bool update_modifiers(int scancode, bool down) {
    if (scancode >= 224 && scancode < 224 + count_of(mods)) {
        uint16_t mod = mods[scancode - 224];
        if (down) {
            modifier_state |= mod;
        } else {
            modifier_state &= ~mod;
        }
        return true;
    }
    return false;
}

#if defined(USE_UART_KEYBOARD) || defined(USE_SDL_EVENT_FORWARDER_INPUT)
void khan_check_uart() {
#ifdef USE_UART_KEYBOARD
    static uint8_t last;
    static uint8_t last_flags;
    static int down_length = 0;
#endif
    if (uart_is_readable(uart_default)) {
        uint8_t flags = 0;
        uint8_t b = uart_getc(uart_default);
        if (skip_key) {
            skip_key = false;
            return;
        }
        uint8_t esc = 0;
        bool menu_key = kms.opacity && !kms.disappearing;
        if (b == 26 && uart_is_readable_within_us(uart_default, 80)) {
            b = uart_getc(uart_default);
            switch (b) {
                case 0:
                    if (uart_is_readable_within_us(uart_default, 80)) {
                        uint scancode = (uint8_t)uart_getc(uart_default);
                        update_modifiers(scancode, true);
                        khan_key_down(scancode,0, modifier_state);
                    }
                    return;
                case 1:
                    if (uart_is_readable_within_us(uart_default, 80)) {
                        uint scancode = (uint8_t)uart_getc(uart_default);
                        update_modifiers(scancode, false);
                        khan_key_up(scancode, 0, modifier_state);
                    }
                    return;
                case 2:
                case 3:
                case 5:
                    if (uart_is_readable_within_us(uart_default, 80)) {

                        uint state = (uint8_t)uart_getc(uart_default);
#ifndef NO_USE_KEMPSTON
                        uint8_t tstate = 0;
                        if (state & 0x03) tstate |= KEMPSTON_F;
                        if (state & 0x10) tstate |= KEMPSTON_U;
                        if (state & 0x20) tstate |= KEMPSTON_D;
                        if (state & 0x40) tstate |= KEMPSTON_L;
                        if (state & 0x80) tstate |= KEMPSTON_R;
                        //printf("Joystick state %02x\n", tstate);
                        khan_set_joystick_state(tstate);
#endif
                    }
                    return;
                case 4:
                    if (uart_is_readable_within_us(uart_default, 80)) {
                        uint __unused scancode = (uint8_t)uart_getc(uart_default);
                    }
                    if (uart_is_readable_within_us(uart_default, 80)) {
                        uint __unused scancode = (uint8_t)uart_getc(uart_default);
                    }
                    return;
            }

            uint8_t __unused state = uart_getc(uart_default);
            return;
        }
#if USE_UART_KEYBOARD
        if (b == 27) {
            if (uart_is_readable_within_us(uart_default, 80)) {
                uint n = uart_getc(uart_default);
                if (uart_is_readable_within_us(uart_default, 80)) {
                    if (n == 91) {
                        esc = uart_getc(uart_default);
                        if (menu_key) {
                            switch (esc)
                            {
                                case 'A':
                                    khan_menu_key(MK_UP);
                                    break;
                                case 'B':
                                    khan_menu_key(MK_DOWN);
                                    break;
                                case 'C':
                                    khan_menu_key(MK_RIGHT);
                                    break;
                                case 'D':
                                    khan_menu_key(MK_LEFT);
                                    break;
                            }
                            return;
                        }
                    }
                } else {
                    skip_key = true;
                }
            } else {
                khan_menu_key(MK_ESCAPE);
                return;
            }
        }
        if (!menu_key)
        {
            uint8_t translated = UartTranslateKey(b, esc, &flags);
            if (translated)
            {
                if (down_length)
                {
                    // can only have one key down
                    khan_zx_key_event(last, last_flags);
                }
                last = translated;
                last_flags = flags;
                down_length = 1;
                flags |= KF_DOWN;
                khan_zx_key_event(last, flags);
            }
        } else {
            if (b == '\r') khan_menu_key(MK_ENTER);
        }
#endif
    } else {
#ifdef USE_UART_KEYBOARD
        if (down_length) {
            if (++down_length == 10) {
                khan_zx_key_event(last, last_flags);
                down_length = 0;
            }
        }
#endif
    }
}
#endif
#endif

#ifndef NO_USE_BEEPER
#if !PICO_ON_DEVICE
void khan_beeper_setup() {}
void khan_beeper_reset() {}
void khan_beeper_enable() {}
void khan_beeper_level_change(uint32_t t, uint8_t level) {}
void khan_beeper_begin_frame(uint32_t t, int32_t frame_num) {}
void khan_beeper_end_frame(uint32_t t) {}
#else

#define BEEPER_ON_PIO1

#ifdef BEEPER_ON_PIO1
#define beeper_pio pio1
#define BEEPER_GPIO_FUNC GPIO_FUNC_PIO1
// why not
#define BEEPER_SM 0
#define BEEPER_DREQ_PIO_TX0 DREQ_PIO1_TX0
#else
#define beeper_pio pio0
#define BEEPER_GPIO_FUNC GPIO_FUNC_PIO0
#define BEEPER_SM 2
#define BEEPER_DREQ_PIO_TX0 DREQ_PIO0_TX0
#endif

#ifndef PICO_AUDIO_PWM_L_PIN
#error need a pin for the beeper
#endif
#define BEEPER_OUTPUT_PIN PICO_AUDIO_PWM_L_PIN
#define BEEPER_PATTERN_LENGTH 16
#define BEEPER_DMA_CHANNEL 3
bi_decl(bi_1pin_with_name(BEEPER_OUTPUT_PIN, "Beeper (PWM)"));

#define BEEPER_MAKE_CYCLE_RUN(pattern, count) (((pattern)<<16)|((count)-1))
void beeper_dma_complete();

#define BEEPER_FRAME_DELAY 2u
#define BEEPER_BUFFER_SIZE_LOG2 9u
#define BEEPER_BUFFER_SIZE (1u << BEEPER_BUFFER_SIZE_LOG2)
#define BEEPER_BUFFER_SIZE_MASK (BEEPER_BUFFER_SIZE - 1u)

// we want two queued, one active, and some jitter due to 50/60 pulldown and one as a buffer
#define BEEPER_FRAMES 4u

static struct {
#ifndef HACK_NON_CIRCULAR
    uint32_t  __attribute__ ((aligned (BEEPER_BUFFER_SIZE * 4))) circular_buffer[BEEPER_BUFFER_SIZE];
#else
    uint32_t  __attribute__ ((aligned (BEEPER_BUFFER_SIZE * 2 * 4))) circular_buffer[BEEPER_BUFFER_SIZE*2];
#endif
    volatile spin_lock_t *spin_lock;
    struct {
        struct beeper_frame {
            int32_t frame_number;
            uint16_t start_pos;
            uint16_t data_length;
        } frames[BEEPER_FRAMES];
        struct beeper_frame *write_frame; // the frame being written, or about to be
        struct beeper_frame *read_frame; // the frame being played, or the next to be
        int32_t next_write_frame_number;
        uint16_t write_limit_pos; // the location that may be written to (we need to start discarding data)
        uint8_t queued_sequential_frame_count; // not including the playing frame
        uint8_t playing_frame_count; // 0 or 1 for whether we have a frame in use
    } locked;
    struct {
        uint32_t last_t;
        uint32_t net_cycles_remainder;
#ifndef NDEBUG
        int32_t frame_tstates;
#endif
#ifdef HACK_NON_CIRCULAR
        int32_t hack_non_circular_offset;
#endif
        uint16_t write_ptr;
        // this is an unlocked copy
        uint16_t write_limit_pos; // the location that may be written to (we need to start discarding data)
        uint16_t last_level;
    } gen_thread;
} beeper_state;

inline static void khan_beeper_start_dma_locked() {
    beeper_assert(beeper_state.locked.queued_sequential_frame_count > 0);
    beeper_assert(!beeper_state.locked.playing_frame_count);
    beeper_assert(beeper_state.locked.read_frame->data_length > 0);
    DEBUG_PINS_SET(beeper, 4);
    DEBUG_PINS_CLR(beeper, 1);
//    printf("begin dma from read_buffer frame %d %p off %d len %d %d\n", beeper_state.locked.read_frame->frame_number, beeper_state.locked.read_frame, beeper_state.locked.read_frame->start_pos, beeper_state.locked.read_frame->data_length, CORE_NUM);
    const uint32_t *buffer = beeper_state.circular_buffer + beeper_state.locked.read_frame->start_pos;
//    printf("%08x %04x\n", (intptr_t)buffer, beeper_state.locked.read_frame->data_length);
    dma_channel_transfer_from_buffer_now(BEEPER_DMA_CHANNEL, (void *)buffer, beeper_state.locked.read_frame->data_length);
    beeper_state.locked.queued_sequential_frame_count--;
    beeper_state.locked.playing_frame_count = 1;
    DEBUG_PINS_SET(beeper, 1);
    DEBUG_PINS_CLR(beeper, 4);
}

void khan_beeper_reset() {
    uint32_t save = spin_lock_blocking(beeper_state.spin_lock);

    beeper_state.locked.write_frame = beeper_state.locked.read_frame;
    if (beeper_state.locked.playing_frame_count) {
        beeper_state.locked.write_frame++;
        if (beeper_state.locked.write_frame == beeper_state.locked.frames + BEEPER_FRAMES) {
            beeper_state.locked.write_frame = beeper_state.locked.frames;
        }
    }
    beeper_state.locked.queued_sequential_frame_count = 0;
    spin_unlock(beeper_state.spin_lock, save);
}

void khan_beeper_begin_frame(uint32_t t, int32_t frame_num) {
    DEBUG_PINS_SET(beeper, 1);
//    printf("Begin frame %d %d\n", frame_num, CORE_NUM);
    uint32_t save = spin_lock_blocking(beeper_state.spin_lock);
    // may be overkill, but right now if we are out of frames or out of order, we discard everything (except what is playing if anything) and start over
    // removed this since it doesn't seem to make much difference, so why waste code... we shouldn't be out of whack anyway unless we are running unsynced
    // note also that we only shift the wrong audio data a frame or two in either direction anyway
//    if ((beeper_state.locked.queued_sequential_frame_count > 0 && frame_num != beeper_state.locked.next_write_frame_number) || (beeper_state.locked.queued_sequential_frame_count == BEEPER_FRAMES - beeper_state.locked.playing_frame_count)) {
    if (beeper_state.locked.queued_sequential_frame_count == BEEPER_FRAMES - beeper_state.locked.playing_frame_count) {
//        printf("Clearing frame queued = %d, wfn = %d\n", beeper_state.locked.queued_sequential_frame_count, beeper_state.locked.next_write_frame_number);
        // discard the last frame we generated - this will give us the shortest gap
        beeper_state.locked.write_frame--;
        if (beeper_state.locked.write_frame < beeper_state.locked.frames) {
            beeper_state.locked.write_frame += BEEPER_FRAMES;
        }
        beeper_assert(beeper_state.locked.queued_sequential_frame_count>0);
        beeper_state.gen_thread.write_ptr = beeper_state.locked.write_frame->start_pos;
        beeper_state.locked.queued_sequential_frame_count--;
    }
    beeper_state.locked.write_frame->frame_number = frame_num;
    beeper_state.locked.write_frame->start_pos = beeper_state.gen_thread.write_ptr;
#ifdef HACK_NON_CIRCULAR
    beeper_state.gen_thread.hack_non_circular_offset = 0;
#endif
#ifndef NDEBUG
    beeper_state.locked.write_frame->data_length = 0;
#endif
    // copy limit into gen thread
    beeper_state.gen_thread.write_limit_pos = beeper_state.locked.write_limit_pos;
    if (!beeper_state.locked.playing_frame_count && beeper_state.locked.queued_sequential_frame_count >= BEEPER_FRAME_DELAY) {
        // dma underflowed (or hadn't started) and we now have enough frames worth of audio (again)
        khan_beeper_start_dma_locked();
    }
    spin_unlock(beeper_state.spin_lock, save);
    DEBUG_PINS_CLR(beeper, 1);
    beeper_state.gen_thread.last_t = t;
}

void khan_beeper_end_frame(uint32_t t) {
#if 0
//    printf("End frame %d %d\n", t, CORE_NUM);
    for(int i=1; i<16; i++) {
        khan_beeper_level_change((t*i)/16, i&1?200:0);
    }
    khan_beeper_level_change(t, 0);
#else
    khan_beeper_level_change(t, beeper_state.gen_thread.last_level);
#endif
    uint32_t save = spin_lock_blocking(beeper_state.spin_lock);
    // finish out the write frame
    if (beeper_state.gen_thread.write_ptr != beeper_state.locked.write_frame->start_pos) {
        beeper_state.locked.write_frame->data_length =
                (BEEPER_BUFFER_SIZE + beeper_state.gen_thread.write_ptr - beeper_state.locked.write_frame->start_pos) &
                BEEPER_BUFFER_SIZE_MASK;
//        printf("%04x %04x %04x\n", beeper_state.gen_thread.write_ptr, beeper_state.locked.write_frame->start_pos, beeper_state.locked.write_frame->data_length);
        beeper_state.locked.next_write_frame_number = beeper_state.locked.write_frame->frame_number + 1;
        // and update
        beeper_state.locked.write_frame++;
        if (beeper_state.locked.write_frame == beeper_state.locked.frames + BEEPER_FRAMES) {
            beeper_state.locked.write_frame = beeper_state.locked.frames;
        }
        beeper_state.locked.queued_sequential_frame_count++;
    } else {
        // we were totally full
    }
    spin_unlock(beeper_state.spin_lock, save);
}

// called when dma is complete
void khan_beeper_next_frame_needed() {
    DEBUG_PINS_SET(beeper, 2);
    uint32_t save = spin_lock_blocking(beeper_state.spin_lock);
//    printf("Begin DMA needed %d\n", CORE_NUM);
    beeper_assert(beeper_state.locked.playing_frame_count);
    beeper_state.locked.playing_frame_count = 0;

    // move past frame we just finished playing
    beeper_state.locked.write_limit_pos = (beeper_state.locked.write_limit_pos + beeper_state.locked.read_frame->data_length) & BEEPER_BUFFER_SIZE_MASK;
    beeper_state.locked.read_frame++;
    if (beeper_state.locked.read_frame == beeper_state.locked.frames + BEEPER_FRAMES) {
        beeper_state.locked.read_frame = beeper_state.locked.frames;
    }

    // if we have any queued sequential frames, then play the next one
    if (beeper_state.locked.queued_sequential_frame_count > 0) {
        beeper_assert(beeper_state.locked.write_limit_pos == ((beeper_state.locked.read_frame->start_pos + BEEPER_BUFFER_SIZE_MASK) & BEEPER_BUFFER_SIZE_MASK));
        khan_beeper_start_dma_locked();
    }
    spin_unlock(beeper_state.spin_lock, save);
    DEBUG_PINS_CLR(beeper, 2);
}

/**
 *
 * start of frame (play a buffered frame if we have two sequential) and we aren't playing... also discard any out of sequence frames
 * dma complete (play a buffered frame if we have one in sequence) otherwise underflow
 * end of frame ... finish book-keeping
 */

void khan_beeper_level_change(uint32_t t, uint8_t level) {
//    printf("Level %d %d %d\n", t, level, CORE_NUM);
    // our thread is the only one writing into the circular buffer
    // todo right now loading a .z80 may cause this - actually shouldn't any longer
    int32_t delta_t = t - beeper_state.gen_thread.last_t;
    beeper_assert(delta_t >= 0);

    // div 3200 because they are all multiples, and it makes 32bit math ok
    static const uint32_t tstates_per_second_div_3200 = 71680 * 50 / 3200;
    static const uint32_t net_clock_divisor = tstates_per_second_div_3200 * (BEEPER_PATTERN_LENGTH * 2 + 5);
    static const uint32_t clocks_per_second_div_3200 = CLOCK_MHZ / 3200;
    static_assert((CLOCK_MHZ / 3200) * (uint64_t)(CLOCK_MHZ / 3200) <= 0x100000000LL, "");

    uint32_t net_cycles_dividend = clocks_per_second_div_3200 * delta_t + beeper_state.gen_thread.net_cycles_remainder;
    int32_t net_cycles = net_cycles_dividend / net_clock_divisor;
    // todo timing still a little off; we're betting off being a little short
    beeper_state.gen_thread.net_cycles_remainder = net_cycles_dividend % net_clock_divisor;
//    pantso = true;
//    if (pantso) printf("%d %d %d %d %d %d\n", skinker, t, delta_t, net_cycles, level, beeper_state.gen_thread.last_level);

    beeper_state.gen_thread.last_t = t;
//    uint32_t pattern = beeper_state.gen_thread.last_level ? 0xfff : 0; // todo volume   65535 >> (__builtin_clz(level)-16);
    uint32_t pattern = (1 << (beeper_state.gen_thread.last_level >> 4)) - 1;

    while (net_cycles > 0) {
        uint32_t count = (net_cycles >> 16) ? 0x10000 : net_cycles;
        if (beeper_state.gen_thread.write_ptr != beeper_state.gen_thread.write_limit_pos) {
#ifndef HACK_NON_CIRCULAR
            beeper_state.circular_buffer[beeper_state.gen_thread.write_ptr] = BEEPER_MAKE_CYCLE_RUN(pattern, count);
            beeper_state.gen_thread.write_ptr = (beeper_state.gen_thread.write_ptr + 1) & BEEPER_BUFFER_SIZE_MASK;
#else
            beeper_state.circular_buffer[beeper_state.gen_thread.write_ptr + beeper_state.gen_thread.hack_non_circular_offset] = BEEPER_MAKE_CYCLE_RUN(pattern, count);
            beeper_state.gen_thread.write_ptr = beeper_state.gen_thread.write_ptr + 1;
            if (beeper_state.gen_thread.write_ptr == BEEPER_BUFFER_SIZE) {
                assert(!beeper_state.gen_thread.hack_non_circular_offset);
                beeper_state.gen_thread.hack_non_circular_offset = BEEPER_BUFFER_SIZE;
                beeper_state.gen_thread.write_ptr = 0;
            }
#endif
            net_cycles -= count;
        } else {
            // underflow
            //printf("pants\n");
            break;
        }
    }
    beeper_state.gen_thread.last_level = level;
#ifndef NDEBUG
    beeper_state.gen_thread.frame_tstates += delta_t;
#endif
}

void __maybe_in_ram khan_beeper_setup() {
    beeper_state.spin_lock = spin_lock_init(spin_lock_claim_unused(true));
    gpio_set_function(BEEPER_OUTPUT_PIN, BEEPER_GPIO_FUNC);

     // todo did this need to be at 22?
//    int beeper_load_offset = 22;
    uint beeper_load_offset = pio_add_program(beeper_pio, &beeper_program);

    pio_sm_config smc = beeper_program_get_default_config(beeper_load_offset);
    sm_config_set_out_shift(&smc, true, false, BEEPER_PATTERN_LENGTH);
    // todo i don't think we need set pins, so I didn't but they were there before
    sm_config_set_out_pins(&smc, BEEPER_OUTPUT_PIN, 1);
    pio_sm_init(beeper_pio, BEEPER_SM, beeper_load_offset, &smc);

    pio_sm_set_consecutive_pindirs(beeper_pio, BEEPER_SM, BEEPER_OUTPUT_PIN, 1, true);
    pio_sm_set_pins(beeper_pio, BEEPER_SM, 0);
//    pio_sm_exec(beeper_pio, BEEPER_SM, pio_encode_set(pio_pindirs, 1)); // Raise the output enable on bottom pin
//    pio_sm_exec(beeper_pio, BEEPER_SM, pio_encode_set(pio_pins, 0)); // clear pin
//    pio_sm_exec(beeper_pio, BEEPER_SM, pio_encode_jmp(beeper_offset_entry_point + beeper_load_offset)); // jmp to ep

//    int ring_bits = 33 - __builtin_clz(BEEPER_BUFFER_SIZE);
#ifndef HACK_NON_CIRCULAR
    int ring_bits = BEEPER_BUFFER_SIZE_LOG2 + 2;
#else
    int ring_bits = 0;
#endif

    dma_channel_config dcc = dma_channel_get_default_config(BEEPER_DMA_CHANNEL);
    channel_config_set_dreq(&dcc, BEEPER_DREQ_PIO_TX0 + BEEPER_SM);
    channel_config_set_ring(&dcc, false, ring_bits);
    dma_channel_set_write_addr(BEEPER_DMA_CHANNEL, &beeper_pio->txf[BEEPER_SM], false);
    dma_channel_set_config(BEEPER_DMA_CHANNEL, &dcc, false);

    irq_set_priority(DMA_IRQ_1, 0x80); // lower priority by 2

    beeper_state.locked.read_frame = beeper_state.locked.write_frame = beeper_state.locked.frames;
    beeper_state.locked.write_limit_pos = BEEPER_BUFFER_SIZE_MASK;
}

void khan_beeper_enable() {
    dma_channel_set_irq1_enabled(BEEPER_DMA_CHANNEL, 1);
    pio_sm_set_enabled(beeper_pio, BEEPER_SM, true);
    irq_set_enabled(DMA_IRQ_1, true);
}

extern void delegate_pwm_irq();

//void __isr __time_critical beeper_dma_complete()
void __maybe_in_ram __isr isr_dma_1()
{
    // note we don't loop so as not to steal time from video (we can certainly wait for another irq)
    if (dma_hw->ints1 & (1u << BEEPER_DMA_CHANNEL)) {
        dma_hw->ints1 = 1u << BEEPER_DMA_CHANNEL;
        khan_beeper_next_frame_needed();
    }
#ifndef NO_USE_AY
    delegate_pwm_irq();
#endif
}
#endif
#endif

#ifdef USE_KHAN_GPIO
bool gpio_allowed(int gpio) {
    return gpio >= PICO_DEBUG_PIN_BASE;
}

uint8_t khan_gpio_read(uint reg) {
    int gpio = (reg&0x1f);
    if (gpio_allowed(reg)) {
        return gpio_get(gpio);
    }
    return 0;
}

void khan_gpio_write(uint reg, uint8_t value) {
    int gpio = (reg&0x1fu);
    if (gpio_allowed(gpio)) {
        if (reg & 0x80) {
            gpio_init(gpio);
            gpio_set_dir(gpio, GPIO_OUT);
            gpio_set_function(gpio, GPIO_FUNC_SIO);
        } else {
            gpio_put(gpio, value);
        }
    }
}
#endif

#if USE_USB

#define MAX_REPORT  4
#define debug_printf(fmt,...) ((void)0)
//#define debug_printf printf

// Each HID instance can has multiple reports
static struct
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    debug_printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    debug_printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);
//    printf("%d USB: device %d connected, protocol %s\n", time_us_32() - t0 , dev_addr, protocol_str[itf_protocol]);

    // By default host stack will use activate boot protocol on supported interface.
    // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
    if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
    {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        debug_printf("HID has %u reports \r\n", hid_info[instance].report_count);
    }

    // request to receive report
    // tuh_hid_report_received_cb() will be invoked when report is available
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
        debug_printf("Error: cannot request to receive report\r\n");
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    debug_printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    printf("USB: device %d disconnected\n", dev_addr);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            TU_LOG2("HID receive boot keyboard report\r\n");
            process_kbd_report( (hid_keyboard_report_t const*) report );
            break;

#ifdef USE_USB_MOUSE
        case HID_ITF_PROTOCOL_MOUSE:
            TU_LOG2("HID receive boot mouse report\r\n");
            process_mouse_report( (hid_mouse_report_t const*) report );
            break;
#endif

        default:
            // Generic report requires matching ReportID and contents with previous parsed report info
            process_generic_report(dev_addr, instance, report, len);
            break;
    }

    // continue to request to receive report
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
        debug_printf("Error: cannot request to receive report\r\n");
    }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for(uint8_t i=0; i<6; i++)
    {
        if (report->keycode[i] == keycode)  return true;
    }

    return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report)
{
    static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

    // ctrl seems not to be used, so assigning it to be ALT as well
    modifier_state = (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT) ? KMOD_LSHIFT : 0) |
                     (report->modifier & (KEYBOARD_MODIFIER_RIGHTSHIFT) ? KMOD_RSHIFT : 0) |
                     (report->modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_LEFTCTRL) ? KMOD_LALT : 0) |
                     (report->modifier & (KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_RIGHTCTRL) ? KMOD_RALT : 0);
    //------------- example code ignore control (non-printable) key affects -------------//
    for(uint8_t i=0; i<6; i++)
    {
        if ( report->keycode[i] )
        {
            if ( find_key_in_report(&prev_report, report->keycode[i]) )
            {
                // exist in previous report means the current key is holding
            }else
            {
                // not existed in previous report means the current key is pressed
                khan_key_down(report->keycode[i], 0, modifier_state);
//                bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
//                pico_key_down(report->keycode[i], 0, is_shift ? WITH_SHIFT : 0);
            }
        }
        // Check for key depresses (i.e. was present in prev report but not here)
        if (prev_report.keycode[i]) {
            // If not present in the current report then depressed
            if (!find_key_in_report(report, prev_report.keycode[i]))
            {
//                bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
//                pico_key_up(prev_report.keycode[i], 0, is_shift ? WITH_SHIFT : 0);
                khan_key_up(prev_report.keycode[i], 0, modifier_state);
            }
        }
    }
    prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

#ifdef USE_USB_MOUSE
static void process_mouse_report(hid_mouse_report_t const * report)
{
    static hid_mouse_report_t prev_report = { 0 };

    uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
    if ( button_changed_mask & report->buttons)
    {
        debug_printf(" %c%c%c ",
                     report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-',
                     report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
                     report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-');
    }

//    cursor_movement(report->x, report->y, report->wheel);
}
#endif

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) dev_addr;

    uint8_t const rpt_count = hid_info[instance].report_count;
    tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
    tuh_hid_report_info_t* rpt_info = NULL;

    if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
    {
        // Simple report without report ID as 1st byte
        rpt_info = &rpt_info_arr[0];
    }else
    {
        // Composite report, 1st byte is report ID, data starts from 2nd byte
        uint8_t const rpt_id = report[0];

        // Find report id in the arrray
        for(uint8_t i=0; i<rpt_count; i++)
        {
            if (rpt_id == rpt_info_arr[i].report_id )
            {
                rpt_info = &rpt_info_arr[i];
                break;
            }
        }

        report++;
        len--;
    }

    if (!rpt_info)
    {
        debug_printf("Couldn't find the report info for this report !\r\n");
        return;
    }

    // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
    // - Keyboard                     : Desktop, Keyboard
    // - Mouse                        : Desktop, Mouse
    // - Gamepad                      : Desktop, Gamepad
    // - Consumer Control (Media Key) : Consumer, Consumer Control
    // - System Control (Power key)   : Desktop, System Control
    // - Generic (vendor)             : 0xFFxx, xx
    if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
    {
        switch (rpt_info->usage)
        {
#ifdef USE_USB_KEYBOARD
            case HID_USAGE_DESKTOP_KEYBOARD:
                TU_LOG1("HID receive keyboard report\r\n");
                // Assume keyboard follow boot report layout
                process_kbd_report( (hid_keyboard_report_t const*) report );
                break;
#endif
#ifdef USE_USB_MOUSE
            case HID_USAGE_DESKTOP_MOUSE:
                TU_LOG1("HID receive mouse report\r\n");
                // Assume mouse follow boot report layout
                process_mouse_report( (hid_mouse_report_t const*) report );
                break;
#endif

            default: break;
        }
    }
}

void khan_check_usb(void) {
    // todo not needed every time perhaps
    tuh_task();
}
#endif