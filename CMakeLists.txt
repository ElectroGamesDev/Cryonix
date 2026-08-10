cmake_minimum_required(VERSION 3.20)
project(Cryonix LANGUAGES CXX)

set(PLATFORM "WINDOWS" CACHE STRING
    "Target platform: WINDOWS, LINUX, MACOS, IOS, ANDROID, WEB")

string(TOUPPER "${PLATFORM}" PLATFORM)

set(VALID_PLATFORMS WINDOWS LINUX MACOS IOS ANDROID WEB)
list(FIND VALID_PLATFORMS "${PLATFORM}" PLATFORM_INDEX)

if(PLATFORM_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Invalid PLATFORM: ${PLATFORM}\n"
        "Valid options: WINDOWS, LINUX, MACOS, IOS, ANDROID, WEB"
    )
endif()

if (PLATFORM STREQUAL "WEB")
    set(DRACO_JS_GLUE ON CACHE BOOL "" FORCE)
    set(DRACO_WASM ON CACHE BOOL "" FORCE)

    # Emscripten flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto" CACHE STRING "" FORCE)
endif()

option(DRACO_JS_GLUE               "Build JavaScript glue" OFF)
option(DRACO_WASM                  "Build WebAssembly" OFF)
option(DRACO_UNITY_PLUGIN          "Build Unity plugin" OFF)
option(DRACO_MAYA_PLUGIN           "Build Maya plugin" OFF)
option(DRACO_TRANSCODER_SUPPORTED  "Enable transcoder" OFF)
option(DRACO_ANIMATION_ENCODING    "Enable animation encoding" OFF)
option(DRACO_TESTS                 "Build Draco tests" OFF)
option(DRACO_BUILD_EXAMPLES        "Build Draco examples" OFF)
option(DRACO_ENCODER               "Enable Draco encoder" OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)


if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "" FORCE)

    set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Os /Gy /GF /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF" CACHE STRING "" FORCE)

    add_compile_options(/Zc:__cplusplus /Zc:preprocessor)
endif()

add_compile_definitions(
    $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>
    $<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>

    $<$<CONFIG:Release>:BX_CONFIG_DEBUG=0>
    $<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>
)


file(GLOB_RECURSE BX_SRC "third_party/bx/src/*.cpp")
add_library(bx STATIC ${BX_SRC})
target_include_directories(bx PUBLIC third_party/bx/include)

file(GLOB_RECURSE BIMG_SRC "third_party/bimg/src/*.cpp")
add_library(bimg STATIC ${BIMG_SRC})
target_include_directories(bimg PUBLIC third_party/bimg/include)

file(GLOB_RECURSE BGFX_SRC "third_party/bgfx/src/*.cpp")
add_library(bgfx STATIC ${BGFX_SRC})
target_include_directories(bgfx PUBLIC third_party/bgfx/include)
target_link_libraries(bgfx PUBLIC bx bimg)

file(GLOB_RECURSE IMGUI_SRC "third_party/imgui/*.cpp")
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC third_party/imgui)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE third_party/stb_image)

if (NOT TARGET stb)
    add_library(stb INTERFACE)
    target_link_libraries(stb INTERFACE stb_image)
endif()

add_library(tinystl INTERFACE)
target_include_directories(tinystl INTERFACE third_party/tinystl)

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE third_party/cgltf)

if (NOT TARGET glm)
    add_library(glm INTERFACE)
    target_include_directories(glm INTERFACE third_party/glm)
endif()

add_subdirectory(third_party/draco)

add_library(rapidobj INTERFACE)
target_include_directories(rapidobj INTERFACE third_party/rapidobj)

file(GLOB_RECURSE UFBX_SRC "third_party/ufbx/*.c")
add_library(ufbx STATIC ${UFBX_SRC})
target_include_directories(ufbx PUBLIC third_party/ufbx)

file(GLOB_RECURSE ZSTD_SRC "third_party/zstd/*.c")
add_library(zstd STATIC ${ZSTD_SRC})
target_include_directories(zstd PUBLIC third_party/zstd)

file(GLOB_RECURSE BASISU_SRC "third_party/basis universal/*.cpp")
add_library(basisu STATIC ${BASISU_SRC})
target_include_directories(basisu PUBLIC "third_party/basis universal")

file(GLOB_RECURSE MINIAUDIO_SRC "third_party/miniaudio/miniaudio.c")
add_library(miniaudio STATIC ${MINIAUDIO_SRC})
target_include_directories(miniaudio PUBLIC third_party/miniaudio)

add_subdirectory(third_party/glfw)


file(GLOB_RECURSE CRYONIX_SRC "src/*.cpp" "src/*.c")
file(GLOB_RECURSE CRYONIX_INCLUDE "include/*.h" "include/*.hpp")

add_library(Cryonix STATIC ${CRYONIX_SRC} ${CRYONIX_INCLUDE})

target_include_directories(Cryonix PUBLIC include)

if (PLATFORM STREQUAL "WINDOWS")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_WINDOWS)
elseif (PLATFORM STREQUAL "LINUX")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_LINUX)
elseif (PLATFORM STREQUAL "MACOS")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_MACOS)
elseif (PLATFORM STREQUAL "IOS")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_IOS)
elseif (PLATFORM STREQUAL "ANDROID")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_ANDROID)
elseif (PLATFORM STREQUAL "WEB")
    target_compile_definitions(Cryonix PUBLIC PLATFORM_WEB)
endif()

target_link_libraries(Cryonix
    PUBLIC
        bgfx
        bx
        bimg
        imgui
        glm
        stb
        cgltf
        draco
)

message(STATUS "Cryonix Target Platform: ${PLATFORM}")