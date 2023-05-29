add_library(stream INTERFACE)

target_sources(stream INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/stream.c
        ${CMAKE_CURRENT_LIST_DIR}/stream.h
        )

target_include_directories(stream INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(stream INTERFACE miniz)
if (TARGET hardware_dma)
    target_link_libraries(stream INTERFACE hardware_dma)
endif()
