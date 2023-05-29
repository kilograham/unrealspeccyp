/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "khan_init.h"
#include "khan.pio.h"
#include "../platform/platform.h"
#include "../tools/options.h"

const struct scanvideo_pio_program video_khan = {
#if PICO_ON_DEVICE
        .program = &video_khan_program,
        .adapt_for_mode = video_khan_adapt_for_mode,
        .configure_pio = video_khan_configure_pio
#else
        .id = VIDEO_KHAN_PROGRAM_NAME
#endif
};

const struct scanvideo_pio_program video_khan54 = {
#if PICO_ON_DEVICE
        .program = &video_khan_program,
        .adapt_for_mode = video_khan_adapt_for_mode54,
        .configure_pio = video_khan_configure_pio
#else
        .id = VIDEO_KHAN_PROGRAM_NAME
#endif
};

const struct scanvideo_mode video_mode_khan =
        {
                .default_timing = &vga_timing_640x480_60_default,
                .pio_program = &video_khan,
                .width = 320,
                .height = 240,
                .xscale = 2,
                .yscale = 2,
        };

const struct scanvideo_mode video_mode_khan_tft =
        {
                .default_timing = &vga_timing_wide_480_50,
                .pio_program = &video_khan,
                .width = 400,
                .height = 240,
                .xscale = 2,
                .yscale = 2,
        };


// this one is good actually on dells for 640*576 (use 5/4 for 512x576)
const struct scanvideo_timing video_timing_khan2 =
        {

                .clock_freq = 24000000,

                .h_active = 640,
                .v_active = 579,

                .h_front_porch = 16,
                .h_pulse = 64,
                .h_total = 800,
                .h_sync_polarity = 1,

                .v_front_porch = 3,
                .v_pulse = 10,
                .v_total = 600,
                .v_sync_polarity = 1,

                .enable_clock = 0,
                .clock_polarity = 0,

                .enable_den = 0
        };


const struct scanvideo_mode video_mode_khan2 =
        {
                .default_timing = &video_timing_khan2,
                .pio_program = &video_khan54,
                .width = 256,
                .height = 193,
                .xscale = 2,
                .yscale = 3,
        };

// this one is ok too, though may need vertical positioning (it is always cropped - presumably at 480
const struct scanvideo_timing video_timing_khan3 =
        {

                .clock_freq = 24000000,

                .h_active = 704,
                .v_active = 526,

                .h_front_porch = 18,
                .h_pulse = 70,
                .h_total = 880,
                .h_sync_polarity = 1,

                .v_front_porch = 3,
                .v_pulse = 10,
                .v_total = 545,
                .v_sync_polarity = 1,

                .enable_clock = 0,
                .clock_polarity = 0,

                .enable_den = 0
        };


const struct scanvideo_mode video_mode_khan3 =
        {
                .default_timing = &video_timing_khan3,
                .pio_program = &video_khan,
                .width = 352,
                .height = 263,
                .xscale = 2,
                .yscale = 2,
        };


#if PICO_ON_DEVICE
static uint32_t missing_scanline_data[] = {
        0x801fu | (0x83ffu << 16u),
        0, // to be filled in
        0x0000u | (0x0000u << 16u),
        video_khan_offset_end_of_line
};

pio_sm_config video_khan_configure_pio(PIO pio, uint sm, uint offset) {
    const int SCANLINE_SM = 0;
    pio_sm_config config = video_khan_program_get_default_config(offset);
    scanvideo_default_configure_pio(pio, sm, offset, &config, sm != SCANLINE_SM);
    return config;
}

extern uint32_t cmd_lookup[256];

bool video_khan_adapt_for_mode(const struct scanvideo_pio_program *program, const struct scanvideo_mode *mode,
                               struct scanvideo_scanline_buffer *missing_scanvideo_scanline_buffer, uint16_t *modifiable_instructions) {
    missing_scanline_data[1] = cmd_lookup[0xaa];
    missing_scanvideo_scanline_buffer->data = missing_scanline_data;
    missing_scanvideo_scanline_buffer->data_used = missing_scanvideo_scanline_buffer->data_max = sizeof(missing_scanline_data) / 4;
#if PICO_SCANVIDEO_PLANE_COUNT > 1
    missing_scanvideo_scanline_buffer->data2 = missing_scanline_data;
    missing_scanvideo_scanline_buffer->data2_used = missing_scanvideo_scanline_buffer->data2_max = sizeof(missing_scanline_data) / 4;
#endif
    return true;
}

bool video_khan_adapt_for_mode54(const struct scanvideo_pio_program *program, const struct scanvideo_mode *mode,
                                 struct scanvideo_scanline_buffer *missing_scanvideo_scanline_buffer, uint16_t *modifiable_instructions) {
    assert(false);// we need the delay values
    missing_scanline_data[1] = cmd_lookup[0xaa];
    missing_scanvideo_scanline_buffer->data = missing_scanline_data;
    missing_scanvideo_scanline_buffer->data_used = missing_scanvideo_scanline_buffer->data_max = sizeof(missing_scanline_data) / 4;
    return true;
}
#endif