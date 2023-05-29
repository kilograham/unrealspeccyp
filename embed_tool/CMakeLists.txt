cmake_minimum_required(VERSION 3.13)
project(embed_tool C CXX)
set(CMAKE_CXX_STANDARD 17)

set(SRC_ROOT ..)

add_executable(embed_tool
        main.cpp
        ${SRC_ROOT}/3rdparty/miniz/miniz.c
        ${SRC_ROOT}/3rdparty/miniz/miniz_tinfl.c
        ${SRC_ROOT}/3rdparty/miniz/miniz_tdef.c
        )

target_include_directories(embed_tool PRIVATE
        ${SRC_ROOT}
        ${SRC_ROOT}/3rdparty/miniz
)
