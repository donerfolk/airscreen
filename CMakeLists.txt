cmake_minimum_required(VERSION 3.16)
project(AirScreen VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "AirScreen currently targets Windows only")
endif()

add_definitions(-DWIN32 -D_WIN32 -DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
add_definitions(-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D_REENTRANT)
add_definitions(-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DNOHOLD)
add_definitions(-DHAVE_CONFIG_H -DLIBPLIST_STATIC -DPLIST_210 -DPLIST_230)
add_definitions(-DOPENSSL_API_COMPAT=0x10101000L)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# OpenSSL: vcpkg, Shining Light, or OPENSSL_ROOT_DIR
if (DEFINED ENV{OPENSSL_ROOT_DIR} AND NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}")
endif()
if (NOT OPENSSL_ROOT_DIR)
    foreach (_p
        "C:/Program Files/OpenSSL-Win64"
        "C:/Program Files/OpenSSL"
        "C:/OpenSSL-Win64"
        "${CMAKE_SOURCE_DIR}/third_party/vcpkg_installed/x64-windows"
        "${CMAKE_SOURCE_DIR}/third_party/prebuilt/openssl")
        if (EXISTS "${_p}/include/openssl/ssl.h")
            set(OPENSSL_ROOT_DIR "${_p}")
            break()
        endif()
    endforeach()
endif()
find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)

find_package(FFmpeg REQUIRED)

set(UXPLAY_DIR "${CMAKE_SOURCE_DIR}/third_party/uxplay")
set(PLIST_DIR "${CMAKE_SOURCE_DIR}/third_party/libplist")
set(COMPAT_DIR "${CMAKE_SOURCE_DIR}/third_party/win_compat")

# --- libplist (binary + XML, no libxml2) ---
file(GLOB PLIST_SRC
    "${PLIST_DIR}/src/plist.c"
    "${PLIST_DIR}/src/bplist.c"
    "${PLIST_DIR}/src/xplist.c"
    "${PLIST_DIR}/src/jplist.c"
    "${PLIST_DIR}/src/oplist.c"
    "${PLIST_DIR}/src/hashtable.c"
    "${PLIST_DIR}/src/bytearray.c"
    "${PLIST_DIR}/src/ptrarray.c"
    "${PLIST_DIR}/src/time64.c"
    "${PLIST_DIR}/src/base64.c"
    "${PLIST_DIR}/src/common.c"
    "${PLIST_DIR}/src/jsmn.c"
    "${PLIST_DIR}/src/out-default.c"
    "${PLIST_DIR}/src/out-limd.c"
    "${PLIST_DIR}/src/out-plutil.c"
    "${PLIST_DIR}/libcnary/node.c"
    "${PLIST_DIR}/libcnary/node_list.c"
)
add_library(plist STATIC ${PLIST_SRC})
target_include_directories(plist PUBLIC
    "${PLIST_DIR}/include"
    "${PLIST_DIR}"
    "${PLIST_DIR}/src"
    "${PLIST_DIR}/libcnary/include"
)
target_compile_definitions(plist PUBLIC LIBPLIST_STATIC HAVE_CONFIG_H)

# --- UxPlay protocol pieces ---
file(GLOB PLAYFAIR_SRC "${UXPLAY_DIR}/lib/playfair/*.c")
add_library(playfair STATIC ${PLAYFAIR_SRC})
target_include_directories(playfair PUBLIC "${UXPLAY_DIR}/lib/playfair")
if (MSVC)
    target_compile_options(playfair PRIVATE /bigobj /wd4244 /wd4267)
endif()

file(GLOB LLHTTP_SRC "${UXPLAY_DIR}/lib/llhttp/*.c")
add_library(llhttp STATIC ${LLHTTP_SRC})
target_include_directories(llhttp PUBLIC "${UXPLAY_DIR}/lib/llhttp")

file(GLOB MDNSD_SRC "${UXPLAY_DIR}/lib/mdnsd/*.c")
add_library(dnssd STATIC ${MDNSD_SRC})
target_include_directories(dnssd PUBLIC "${COMPAT_DIR}" "${UXPLAY_DIR}/lib/mdnsd" "${UXPLAY_DIR}/lib")
target_link_libraries(dnssd PUBLIC ws2_32 iphlpapi)

file(GLOB AIRPLAY_SRC "${UXPLAY_DIR}/lib/*.c")
add_library(airplay STATIC ${AIRPLAY_SRC} "${COMPAT_DIR}/win_compat.c")
target_include_directories(airplay PUBLIC
    "${COMPAT_DIR}"
    "${UXPLAY_DIR}/lib"
    "${UXPLAY_DIR}/lib/playfair"
    "${UXPLAY_DIR}/lib/llhttp"
    "${PLIST_DIR}/include"
)
target_link_libraries(airplay PUBLIC
    playfair
    llhttp
    dnssd
    plist
    OpenSSL::Crypto
    ws2_32
    iphlpapi
    crypt32
    bcrypt
)
if (MSVC)
    target_compile_options(airplay PRIVATE /wd4244 /wd4267 /wd4018 /wd4996 /FI"${COMPAT_DIR}/airscreen_types.h")
    target_compile_options(plist PRIVATE /wd4244 /wd4267 /wd4996 /wd4018 /FI"${COMPAT_DIR}/airscreen_types.h")
    target_compile_options(dnssd PRIVATE /wd4244 /wd4267 /FI"${COMPAT_DIR}/airscreen_types.h")
endif()

if (MSVC)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_SOURCE_DIR}/src/app\"")
    set_property(SOURCE src/app/airscreen.rc APPEND PROPERTY
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/app/app.ico")
endif()

# --- AirScreen app ---
add_executable(AirScreen WIN32
    src/main.cpp
    src/receiver.cpp
    src/video.cpp
    src/audio.cpp
    src/settings.cpp
    src/ui.cpp
    src/openssl_applink.c
    src/app/airscreen.rc
)
target_include_directories(AirScreen PRIVATE src src/app "${UXPLAY_DIR}/lib" "${COMPAT_DIR}")
target_compile_definitions(AirScreen PRIVATE AIRSCREEN_VERSION="${PROJECT_VERSION}" UNICODE _UNICODE)
target_link_libraries(AirScreen PRIVATE
    airplay
    FFmpeg::FFmpeg
    d3d11
    dxgi
    d3dcompiler
    ole32
    oleaut32
    uuid
    mfplat
    mfuuid
    strmiids
    winmm
    shell32
    user32
    gdi32
    comctl32
    advapi32
    dwmapi
    gdiplus
)
if (MSVC)
    set_source_files_properties(
        src/main.cpp src/receiver.cpp src/video.cpp src/audio.cpp src/settings.cpp
        PROPERTIES COMPILE_OPTIONS "/utf-8;/W3;/wd4996;/FI${COMPAT_DIR}/airscreen_types.h")
    set_source_files_properties(src/ui.cpp PROPERTIES COMPILE_OPTIONS "/utf-8;/W3")
    # Keep OPENSSL_Applink in the EXE export table (OpenSSL GetProcAddress).
    target_link_options(AirScreen PRIVATE /MANIFEST:NO /EXPORT:OPENSSL_Applink)
endif()

# Copy FFmpeg (and OpenSSL) DLLs next to the exe
if (FFMPEG_BIN_DIR)
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    foreach (_dll IN LISTS FFMPEG_DLLS)
        add_custom_command(TARGET AirScreen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:AirScreen>"
        )
    endforeach()
endif()
if (OPENSSL_ROOT_DIR)
    file(GLOB OPENSSL_DLLS
        "${OPENSSL_ROOT_DIR}/bin/*.dll"
        "${OPENSSL_ROOT_DIR}/*.dll")
    foreach (_dll IN LISTS OPENSSL_DLLS)
        add_custom_command(TARGET AirScreen POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:AirScreen>"
        )
    endforeach()
endif()

install(TARGETS AirScreen RUNTIME DESTINATION .)
if (FFMPEG_BIN_DIR)
    install(DIRECTORY "${FFMPEG_BIN_DIR}/" DESTINATION . FILES_MATCHING PATTERN "*.dll")
endif()
