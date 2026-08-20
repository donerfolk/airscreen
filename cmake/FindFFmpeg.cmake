# Find FFmpeg (avcodec, avutil, swscale, swresample) for MSVC shared builds.
#
# Search order:
#   1. FFMPEG_ROOT CMake/env variable
#   2. ${PROJECT_SOURCE_DIR}/third_party/prebuilt/ffmpeg
#   3. system paths

function(_airscreen_ffmpeg_from_root root)
    if (NOT EXISTS "${root}")
        return()
    endif()
    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        PATHS "${root}/include"
        NO_DEFAULT_PATH)
    find_library(FFMPEG_AVCODEC_LIBRARY
        NAMES avcodec avcodec.lib
        PATHS "${root}/lib" "${root}/bin"
        NO_DEFAULT_PATH)
    find_library(FFMPEG_AVUTIL_LIBRARY
        NAMES avutil avutil.lib
        PATHS "${root}/lib" "${root}/bin"
        NO_DEFAULT_PATH)
    find_library(FFMPEG_SWSCALE_LIBRARY
        NAMES swscale swscale.lib
        PATHS "${root}/lib" "${root}/bin"
        NO_DEFAULT_PATH)
    find_library(FFMPEG_SWRESAMPLE_LIBRARY
        NAMES swresample swresample.lib
        PATHS "${root}/lib" "${root}/bin"
        NO_DEFAULT_PATH)
endfunction()

if (DEFINED ENV{FFMPEG_ROOT} AND NOT FFMPEG_ROOT)
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()

if (FFMPEG_ROOT)
    _airscreen_ffmpeg_from_root("${FFMPEG_ROOT}")
endif()

if (NOT FFMPEG_INCLUDE_DIR)
    file(GLOB _ff_candidates "${CMAKE_SOURCE_DIR}/third_party/prebuilt/ffmpeg*")
    list(INSERT _ff_candidates 0 "${CMAKE_SOURCE_DIR}/third_party/prebuilt/ffmpeg")
    foreach (_c IN LISTS _ff_candidates)
        if (EXISTS "${_c}/include/libavcodec/avcodec.h")
            _airscreen_ffmpeg_from_root("${_c}")
            if (FFMPEG_INCLUDE_DIR)
                set(FFMPEG_ROOT "${_c}")
                break()
            endif()
        endif()
        if (EXISTS "${_c}/include/libavcodec/avcodec.h")
            break()
        endif()
        # BtbN zip extracts to ffmpeg-master-latest-win64-gpl-shared/
        file(GLOB _inner "${_c}/*/include/libavcodec/avcodec.h")
        foreach (_h IN LISTS _inner)
            get_filename_component(_inc "${_h}" DIRECTORY) # libavcodec
            get_filename_component(_inc "${_inc}" DIRECTORY) # include
            get_filename_component(_root "${_inc}" DIRECTORY)
            _airscreen_ffmpeg_from_root("${_root}")
            if (FFMPEG_INCLUDE_DIR)
                set(FFMPEG_ROOT "${_root}")
                break()
            endif()
        endforeach()
        if (FFMPEG_INCLUDE_DIR)
            break()
        endif()
    endforeach()
endif()

if (NOT FFMPEG_INCLUDE_DIR)
    find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h)
    find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec)
    find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil)
    find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale)
    find_library(FFMPEG_SWRESAMPLE_LIBRARY NAMES swresample)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS
        FFMPEG_INCLUDE_DIR
        FFMPEG_AVCODEC_LIBRARY
        FFMPEG_AVUTIL_LIBRARY
        FFMPEG_SWSCALE_LIBRARY
        FFMPEG_SWRESAMPLE_LIBRARY)

if (FFmpeg_FOUND AND NOT TARGET FFmpeg::FFmpeg)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    target_include_directories(FFmpeg::FFmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
    target_link_libraries(FFmpeg::FFmpeg INTERFACE
        "${FFMPEG_AVCODEC_LIBRARY}"
        "${FFMPEG_AVUTIL_LIBRARY}"
        "${FFMPEG_SWSCALE_LIBRARY}"
        "${FFMPEG_SWRESAMPLE_LIBRARY}")
    get_filename_component(FFMPEG_BIN_DIR "${FFMPEG_AVCODEC_LIBRARY}" DIRECTORY)
    if (EXISTS "${FFMPEG_BIN_DIR}/../bin")
        get_filename_component(FFMPEG_BIN_DIR "${FFMPEG_BIN_DIR}/../bin" ABSOLUTE)
    endif()
    set(FFMPEG_BIN_DIR "${FFMPEG_BIN_DIR}" CACHE PATH "FFmpeg runtime DLL directory")
endif()
