cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBJXR_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_jxr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/jxr_decode.c
        ${CMAKE_CURRENT_SOURCE_DIR}/jxr_stubs.c
)

# libjxrglue holds the container parser, the pixel-format tables and the format
# converter; libjpegxr is the codec itself. The glue calls into the codec, so it
# has to come first on the link line.
SET(LIBJXR_LIBS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libjxrglue.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libjpegxr.a
)

SET(LIBJXR_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libjxr
        "${LIBJXR_SRC}"
        "${LIBJXR_LIBS}"
        []
        ""
        "${LIBJXR_INC}"
        ""
        "1")
