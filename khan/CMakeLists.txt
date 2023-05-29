add_library(khan_common INTERFACE)

# this doesn't work
OPTION(KHAN128_I2S "(doesn't work) Use I2S rather than PWM for non beeper audio (khan128)")

function(create_embed_file TARGET SOURCE_FILE TARGET_FILE)
    if (NOT EmbedTool_FOUND)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR})
        find_package(EmbedTool)
    endif()

    cmake_parse_arguments(embed "DISC;ROM" "PREFIX" "" ${ARGN} )

    set(EMBED_TARGET "${TARGET}_embed")

    set(EMBED_ARGS "")
    if (embed_ROM)
        LIST(APPEND EMBED_ARGS "-r")
        STRING(APPEND EMBED_TARGET "_roms")
    else()
        LIST(APPEND EMBED_ARGS "-g")
        STRING(APPEND EMBED_TARGET "_games")
    endif()
    if (embed_PREFIX)
        LIST(APPEND EMBED_ARGS "-p" "${embed_PREFIX}")
    endif()
    add_custom_target(${EMBED_TARGET} DEPENDS ${TARGET_FILE})

    add_custom_command(OUTPUT ${TARGET_FILE}
            DEPENDS ${SOURCE_FILE}
            COMMAND EmbedTool ${EMBED_ARGS} ${SOURCE_FILE} ${TARGET_FILE}
            )
    add_dependencies(${TARGET} ${EMBED_TARGET})

    message("COMMAND EmbedTool ${EMBED_ARGS} ${SOURCE_FILE} ${TARGET_FILE}")
    target_sources(${TARGET} PRIVATE ${TARGET_FILE})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
endfunction()

# NEED CUSTOM ALLOCATOR for USB memory

pico_generate_pio_header(khan_common ${CMAKE_CURRENT_LIST_DIR}/khan.pio)

target_sources(khan_common INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/khan.c
        ${CMAKE_CURRENT_LIST_DIR}/khan_init.cpp
        ${CMAKE_CURRENT_LIST_DIR}/khan_lib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ui_font.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../file_type.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../options_common.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../speccy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../speccy_handler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/device.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/memory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/ula.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/input/kempston_joy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/input/keyboard.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/input/tape.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/sound/ay.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/sound/beeper.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../devices/sound/device_sound.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../platform/io.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../platform/platform.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../snapshot/snapshot.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../tools/options.cpp
        )

if (PICO_ON_DEVICE)
    target_sources(khan_common INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/z80arm.cpp
            ${CMAKE_CURRENT_LIST_DIR}/z80khan.S
            ${CMAKE_CURRENT_LIST_DIR}/fast_ay.S
            )
    target_link_libraries(khan_common INTERFACE
            hardware_interp
    )
else()
    target_sources(khan_common INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/spoon.cpp
            ${CMAKE_CURRENT_LIST_DIR}/../z80/z80.cpp
            ${CMAKE_CURRENT_LIST_DIR}/../z80/z80_opcodes.cpp
            ${CMAKE_CURRENT_LIST_DIR}/../z80/z80_op_tables.cpp
            ${CMAKE_CURRENT_LIST_DIR}/z80t.cpp
            )
endif()

# -------------------------------------------------------------------------------
# system wide definitions
# -------------------------------------------------------------------------------
target_compile_definitions(khan_common INTERFACE
        #COLLAPSE_IRQS
        #NO_USE_SIM
        PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=4
        PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=76
        PICO_SCANVIDEO_MAX_SCANLINE_BUFFER2_WORDS=48
        PICO_SCANVIDEO_48MHZ=1
        PICO_HEAP_SIZE=0x400
        PICO_STACK_SIZE=0x380
        ENABLE_COMPRESSED_STREAM
        $<$<BOOL:${PICO_BUILD}>:NDEBUG>
        #PICO_DEBUG_MALLOC
        PICO_USE_OPTIMISTIC_SBRK

        PICO_SCANVIDEO_PLANE_COUNT=2

)

add_library(khan128_core INTERFACE)

if (KHAN128_I2S)
    target_compile_definitions(khan128_core INTERFACE
            PICO_AUDIO_I2S_PIO=1
            PICO_AUDIO_I2S_DMA_IRQ=1
            PICO_AUDIO_I2S_MONO_INPUT=1
            PICO_AUDIO_I2S_MONO_OUTPUT=1
            KHAN128_I2S=1
            # this is a little over enough for 1/50s of audio at 24000Hz
            PICO_AUDIO_BUFFER_SAMPLE_LENGTH=484
            )
else()
    target_compile_definitions(khan128_core INTERFACE
            PICO_AUDIO_PWM_PIO=1
            PICO_AUDIO_PWM_INTERP_SAVE=1
            # todo trying to shrink stuff down a bit - may not work
            PICO_AUDIO_BUFFERS_PER_CHANNEL=2
            # this is a little over enough for 1/50s of audio at 22058Hz
            PICO_AUDIO_BUFFER_SAMPLE_LENGTH=448
            )
endif()
target_compile_definitions(khan128_core INTERFACE
        # for use without NO_USE_BEEPER
        #PIO_PWM_IRQ_FUNC_NAME=delegate_pwm_irq
        BLOCKING_GIVE_SYNCHRONIZE_BUFFERS

        PICO_PRINTF_SUPPORT_FLOAT=0
        PICO_PRINTF_SUPPORT_LONG_LONG=0
        PICO_PRINTF_SUPPORT_PTRDIFF_T=0

        PICO_SCANVIDEO_MISSING_SCANLINE_COLOR=0

        PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1
)

if (PICO_ON_DEVICE)
    target_compile_definitions(khan128_core INTERFACE
            PICO_BUFFER_USB_ALLOC_HACK=1
            )
endif()

# -------------------------------------------------------------------------------
# khan specifics
# -------------------------------------------------------------------------------
target_compile_definitions(khan_common INTERFACE
        USE_MU
        USE_KHAN_GPIO 
        USE_Z80_ARM_OFFSET_T
        USE_STREAM
        USE_MU_SIMPLIFICATIONS
        USE_HACKED_DEVICE_ABSTRACTION
        USE_LARGER_FASTER_CB
        # we still want to be able to load off disk for native
        $<$<BOOL:${PICO_ON_DEVICE}>:USE_EMBEDDED_FILES>

#        ENABLE_BREAKPOINT_IN_DEBUG
#        NO_USE_TAPE
#        NO_USE_FAST_TAPE
#        NO_UPDATE_RLOW_IN_FETCH

        NO_USE_DESTRUCTORS
        # thse two have to be used together actually!
        NO_USE_SCREEN

        NO_USE_FDD
        #NO_USE_KEMPSTON
        NO_USE_REPLAY 
        NO_USE_DOS
        NO_USE_SERVICE_ROM
        NO_USE_SAVE 
        NO_USE_CSW 
        NO_USE_TZX 
        NO_USE_SZX
#        NO_USE_Z80
#        NO_USE_SNA
        )

target_compile_definitions(khan128_core INTERFACE
        #for now don't use both
        NO_USE_BEEPER
        #$<$<BOOL:${PICO_ON_DEVICE}>:NO_USE_AY>
        $<$<BOOL:${PICO_ON_DEVICE}>:USE_FAST_AY>
        USE_OVERLAPPED_ROMS
        # this simply refers to use of banks by the z80 runtime; it is required for NO_USE_128K
        $<$<BOOL:${PICO_ON_DEVICE}>:USE_BANKED_MEMORY_ACCESS>
        )

if (NOT PICO_NO_FLASH)
    target_compile_definitions(khan_common INTERFACE
            USE_COMPRESSED_ROM
            USE_XIP_STREAM
            )
endif()

#target_compile_options(khan_common INTERFACE -Wall -Werror)

add_library(miniz INTERFACE)

set(MINIZ_DIR ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/miniz)
target_sources(miniz INTERFACE
        ${MINIZ_DIR}/miniz_tinfl.c
        )

target_include_directories(miniz INTERFACE ${MINIZ_DIR})

target_link_libraries(khan_common INTERFACE pico_stdlib pico_scanvideo_dpi miniz stream pico_multicore)

function(add_khan_exe NAME GAME_FILE ROM_FILE USE_USB)
    set(USE_USB_KEYBOARD ${USE_USB})
    add_executable(${NAME})
    set(OVERRIDE_GAME_FILE ${NAME}_games)
    if (DEFINED ${OVERRIDE_GAME_FILE})
        set(GAME_FILE ${OVERRIDE_GAME_FILE})
    endif()
    set(OVERRIDE_ROM_FILE ${NAME}_roms)
    if (DEFINED ${OVERRIDE_ROM_FILE})
        set(ROM_FILE ${OVERRIDE_ROM_FILE})
    endif()
    pico_set_float_implementation(${NAME} none)
    pico_set_double_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} 1) # just use raw uart

    get_filename_component(GAME_FILE ${GAME_FILE} ABSOLUTE)

    # todo override prefix
    create_embed_file(${NAME} ${GAME_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${NAME}_games.cpp PREFIX ${CMAKE_CURRENT_LIST_DIR}/../../unrealspeccyp/res)
    get_filename_component(ROM_FILE ${ROM_FILE} ABSOLUTE)
    create_embed_file(${NAME} ${ROM_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${NAME}_roms.cpp PREFIX ${CMAKE_CURRENT_LIST_DIR}/../../unrealspeccyp/res/rom ROM)
    pico_add_extra_outputs(${NAME})

    if (PICO_ON_DEVICE)
        if (USE_USB_KEYBOARD OR USE_USB_MOUSE)
            target_link_libraries(${NAME} PRIVATE tinyusb_host)

            target_compile_definitions(${NAME} PRIVATE
                    USB_DPRAM_MAX=768 # needs to be big
                    PICO_USB_HOST_INTERRUPT_ENDPOINTS=3
                    )
        endif()
        if (USE_USB_KEYBOARD)
            target_compile_definitions(${NAME} PRIVATE USE_USB_KEYBOARD)
        endif()
        if (USE_USB_MOUSE)
            target_compile_definitions(${NAME} PRIVATE USE_USB_MOUSE)
        endif()
        if (NOT PICO_NO_FLASH)
            pico_set_linker_script(${NAME} ${CMAKE_CURRENT_LIST_DIR}/memmap_khan.ld)
        endif()
    endif()
endfunction()

add_khan_exe(khan games/khan_games.txt roms/khan_roms.txt 0)
add_khan_exe(khan128 games/khan128_games.txt roms/khan128_roms.txt 0)
add_khan_exe(khan128_turbo games/khan128_turbo_games.txt roms/khan128_roms.txt 0)
add_khan_exe(khan_usb games/khan_games.txt roms/khan_roms.txt 1)
add_khan_exe(khan128_usb games/khan128_games.txt roms/khan128_roms.txt 1)
add_khan_exe(khan128_turbo_usb games/khan128_turbo_games.txt roms/khan128_roms.txt 1)

target_compile_definitions(khan PRIVATE
        USE_SINGLE_64K_MEMORY # note this is ignored with USE_COMPRESSED_ROM
        NO_USE_128K
        NO_USE_AY
        )
target_compile_definitions(khan_usb PRIVATE
        USE_SINGLE_64K_MEMORY # note this is ignored with USE_COMPRESSED_ROM
        NO_USE_128K
        NO_USE_AY
        )

if (PICO_ON_DEVICE)
#    target_compile_definitions(khan_common INTERFACE USE_UART_KEYBOARD) # regular UART Input
    target_compile_definitions(khan_common INTERFACE USE_SDL_EVENT_FORWARDER_INPUT) # support SDL event forwarder
    target_compile_definitions(khan_common INTERFACE USE_USB_INPUT) # support SDL event forwarder
    target_compile_definitions(khan_common INTERFACE USE_Z80_ARM)
    #    target_compile_definitions(khan_common INTERFACE USE_LEGACY_TAPE_COMPARISON)
endif()

target_compile_options(khan_common INTERFACE -g)
target_link_libraries(khan PRIVATE khan_common)
target_link_libraries(khan_usb PRIVATE khan_common)

if (KHAN128_I2S)
    target_link_libraries(khan128_core INTERFACE khan_common pico_audio_i2s)
else()
    target_link_libraries(khan128_core INTERFACE khan_common pico_audio_pwm)
endif()

target_link_libraries(khan128 PRIVATE khan128_core)
target_link_libraries(khan128_turbo PRIVATE khan128_core)
target_link_libraries(khan128_usb PRIVATE khan128_core)
target_link_libraries(khan128_turbo_usb PRIVATE khan128_core)

target_compile_definitions(khan128_turbo PRIVATE
        SPEEDUP=3
)
target_compile_definitions(khan128_turbo_usb PRIVATE
        SPEEDUP=3
        )
