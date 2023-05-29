# Finds (or builds) the EmbedTool executable
#
# This will define the following variables
#
#    EmbedTool_FOUND
#
# and the following imported targets
#
#     EmbedTool
#

if (NOT EmbedTool_FOUND)
    # todo we would like to use pckgconfig to look for it first
    # see https://pabloariasal.github.io/2018/02/19/its-time-to-do-cmake-right/

    include(ExternalProject)

    set(EmbedTool_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../embed_tool)
    set(EmbedTool_BINARY_DIR ${CMAKE_BINARY_DIR}/embed_tool)

    set(EmbedTool_BUILD_TARGET EmbedToolBuild)
    set(EmbedTool_TARGET EmbedTool)

    if (NOT TARGET ${EmbedTool_BUILD_TARGET})
        message("EmbedTool will need to be built")
        ExternalProject_Add(${EmbedTool_BUILD_TARGET}
                PREFIX embed_tool
                SOURCE_DIR ${EmbedTool_SOURCE_DIR}
                BINARY_DIR ${EmbedTool_BINARY_DIR}
                BUILD_ALWAYS 1 # force dependency checking
                INSTALL_COMMAND ""
                )
    endif()

    set(EmbedTool_EXECUTABLE ${EmbedTool_BINARY_DIR}/embed_tool)
    if(NOT TARGET ${EmbedTool_TARGET})
        add_executable(${EmbedTool_TARGET} IMPORTED)
    endif()
    set_property(TARGET ${EmbedTool_TARGET} PROPERTY IMPORTED_LOCATION
            ${EmbedTool_EXECUTABLE})

    add_dependencies(${EmbedTool_TARGET} ${EmbedTool_BUILD_TARGET})
    set(EmbedTool_FOUND 1)
endif()
