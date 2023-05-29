/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include "z80t.h"

void spoono() {
    Z80t::eZ80t cpu;
    cpu.generate_arm();
}
