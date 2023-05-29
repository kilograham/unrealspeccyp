# Overview

This README is specific to the "pico" fork of unrealspeccyp. See [here](README_USP) for the original unrealspeccyp readme.

## Features

* Spectrum 48K and 128K emulator based on [unrealspeccyp](https://bitbucket.org/djdron/unrealspeccyp/wiki/Home).
* VGA output.
* Beeper output pin for 48K spectrum (i.e. just wiggling the pin like the real Spectrum!).
* AY sound output for 128K spectrum.
* .TAP, .SNA, .Z80 support (embedded in binary).
* This port was development during pre-silicon RP2040 development. Therefore, it had to run at 48Mhz, and indeed.
  both 48K and 128K emulators still run by default at a 48Mhz system clock.
* Read/Write on RP2040 GPIO pins - because why not? (see bitbang app).

## Some caveats

* The sole reason for this project's existence was as a system test/bit of fun during early development of the RP2040, it is 
  not intended to be the best, full-featured emulator. I have open sourced it as people keep asking!
* 'Khan' is the codename I used to keep what I was doing secret :-)
* No doubt these terse instructions will prove frustrating for some; feel free to submit PRs!
* You cannot currently use beeper/tape noise and AY sound at the same time (as they use different mechanisms) - 
  actually I think there is support for using one PWM channel for each, but IDK if it still works. 
* I did some recent cleanup in preparation for open-source:
  * Untangled the horrible mess of symlinks that had this building within another makefile project prior to the Pico 
    SDK.
  * Added USB keyboard support (this seems fine - though I had to hunt for more RAM, so it is possible this may 
    break something - you can run with or without).
  * Hopefully got a license on everything important.
  * Added some I2S support - this is only for AY sound as the beeper just wiggles a pin. Sadly my I2S decoder 
    doesn't like 22050Hz, so I had to up it to 24000Hz. I don't necessarily recommend using this if you can use PWM 
    (the default).
  
# Running

## Pin Outs

If you want to know about pin usage and features, the best thing to do is run:

e.g.
```bash
$ picotool info -a khan128_usb.elf
File khan128_usb.elf:

Program Information
 name:          khan128_usb
 features:      SDL event forwarder support
                USB keyboard support
                UART stdin / stdout
 binary start:  0x10000000
 binary end:    0x1003b2ec

Fixed Pin Information
 0-4:    Red 0-5
 6-10:   Green 0-5
 11-15:  Blue 0-5
 16:     HSync
 17:     VSync
 20:     UART1 TX
 21:     UART1 RX
 28:     AY sound (PWM)

Build Information
 sdk version:       1.5.1-develop
 pico_board:        vgaboard
 boot2_name:        boot2_w25q080
 build date:        May 29 2023
 build attributes:  Release 
```

These video/audio pins match for example the Pimoroni Pico VGA Demo Base which itself is based on the suggested
Raspberry Pi Documentation [here](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf)
and the design files zipped [here](https://datasheets.raspberrypi.com/rp2040/VGA-KiCAD.zip).

## Keyboard input

* **USB Keyboard** - Depending on the version you have, you can use a real USB keyboard (if supported by TinyUSB on 
  RP2040). I know 
RaspberryPi keyboards work, and my old Dell keyboard. If in doubt, the older and crappier the better! USB keyboard 
  support is enabled for binaries with names ending in `_usb`.

* **SDL Event Forwader** - This is a simple app that tunnels input from a window on the host machine over UART to 
  the RP2040: https://github.com/kilograham/sdl_event_forwarder. Note however this sometimes seems to get keys stuck 
  on the spectrum. This is enabled by default in all builds.

* **Raw UART** - Not very useful, but you can enable this, and allow typing a few keys on the spectrum (you need to 
  find this in the `CMakeLists.txt` to enable it).

### Keys

These are the same as regular unrealspeccyp. Good luck typing!
* Shift keys on your keyboard are "Caps Shift".
* Alt or Ctrl keys on your keyboard are "Symbol Shift".

# Building

You should use the latest `develop` branch of 
[pico-sdk](https://github.com/raspberrypi/pico-sdk/tree/develop) and
[pico-extras](https://github.com/raspberrypi/pico-extras/tree/develop).

## Building for RP2040

This should work on Linux and Mac (and possibly Windows; I haven't tried).

To build, from the project root directory:
```
mkdir build
cd build
cmake -DPICO_SDK_PATH=path/to/pico-sdk -DPICO_BOARD=vgaboard ..
make -j4
```

## Building for RP2040

This should work on Linux and Mac (and possibly Windows; I haven't tried).

To build:
```
mkdir build
cd build
cmake -DPICO_SDK_PATH=path/to/pico-sdk -DPICO_BOARD=vgaboard ..
make -j4
```

You need to specify `PICO_EXTRAS_PATH` as well if `pico-extras` isn't in a sibling directory of `pico-sdk`.

Note the output binaries are in `build/khan`.

## Building for PICO_PLATFORM=host

You can run this on your Linux/macOS host if you have `libsdl2-dev` etc. installed using the `host` platform
mode of the SDK.

You need [pico_host_sdl](https://github.com/raspberrypi/pico-host-sdl) to replace the video/audio support with native. 

To build:
```
mkdir native_build
cd native_build
cmake -DPICO_SDK_PATH=path/to/pico-sdk -DPICO_PLATFORM=host -DPICO_SDK_PRE_LIST_DIRS=/path/to/pico_host_sdl  ..
make -j4
```

Note the output binaries are in `native_build/khan`. There is no different between `_usb` variants and non `_usb` 
variants.

# Implementation Notes

Not much here atm, sorry:

* We have A really quite small/fast Z80 emulator - this might be useful
  * The generation is a bit batshit; I figured, at the time, that the simplest thing to do would be to use some C++ 
    hackery, to execute the code for each instruction, using crafted C++ classes, such that assignments etc. are 
    overloaded to print out ARM assembly. (shiver) 
  * If I recalled, the smallest version of the code is 2K, the slightly bigger one is 3K.
* The beeper/tape implementation just wiggles a pin, which it tries to do at exactly the right time by passing run 
  lengths to a PIO state machine. Note that to support volume, a short PWM pulse is repeated for each length of the run.
* The small AY emulator is also quite handy, though again is a bit crazy, because I thought it would be important to 
  do some oversampling, and frankly, I'm no longer sure that is necessary. Also, I wonder if it is missing some 
  feature or other (I don't recall), as the `down` demo in `kahn_turbo` sounds different on native.
* I didn't make a proper port in the platform/ directory, sorry. This is probably possible, but wasn't my focus.
