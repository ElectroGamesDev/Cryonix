cmake_minimum_required(VERSION 3.20)
project(Cryonix LANGUAGES CXX C)

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

if(NOT PLATFORM STREQUAL "WINDOWS")
    message(FATAL_ERROR
        "Only the WINDOWS platform is currently supported. "
        "The vendored third-party libraries are prebuilt for Windows only."
    )
endif()

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
    PLATFORM_WINDOWS

    $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>
    $<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>

    $<$<CONFIG:Release>:BX_CONFIG_DEBUG=1>
    $<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>
)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${THIRD_PARTY_DIR}/cgltf/include)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${THIRD_PARTY_DIR}/stb_image/include)

add_library(rapidobj INTERFACE)
target_include_directories(rapidobj INTERFACE ${THIRD_PARTY_DIR})

add_library(compat INTERFACE)
target_include_directories(compat INTERFACE ${THIRD_PARTY_DIR}/compat/include)

add_library(draco INTERFACE)
target_include_directories(draco INTERFACE ${THIRD_PARTY_DIR})

file(GLOB_RECURSE CRYONIX_SRC "src/*.cpp" "src/*.c")
file(GLOB_RECURSE CRYONIX_INCLUDE "include/*.h" "include/*.hpp")

add_library(Cryonix STATIC
    ${CRYONIX_SRC}
    ${CRYONIX_INCLUDE}
    ${THIRD_PARTY_DIR}/ufbx/ufbx.c
    ${THIRD_PARTY_DIR}/zstd/zstddeclib.c
    "${THIRD_PARTY_DIR}/basis universal/basisu_transcoder.cpp"
)

target_include_directories(Cryonix PUBLIC include)

# Don't add third_party/bx or third_party/tinystl here: they contain math.h/string.h that shadow the MSVC ones
target_include_directories(Cryonix PUBLIC
    ${THIRD_PARTY_DIR}/bgfx/include
    ${THIRD_PARTY_DIR}/bgfx/include/c99
    ${THIRD_PARTY_DIR}/bimg/include
    ${THIRD_PARTY_DIR}/cgltf/include
    ${THIRD_PARTY_DIR}/compat/include
    ${THIRD_PARTY_DIR}/glfw/include
    ${THIRD_PARTY_DIR}/stb_image/include
    ${THIRD_PARTY_DIR}
)

target_link_libraries(Cryonix
    PUBLIC
        ${THIRD_PARTY_DIR}/bgfx/lib/bgfxDebug.lib
        ${THIRD_PARTY_DIR}/bx/lib/bxDebug.lib
        ${THIRD_PARTY_DIR}/bimg/lib/bimgDebug.lib
        ${THIRD_PARTY_DIR}/bimg/lib/bimg_decodeDebug.lib
        ${THIRD_PARTY_DIR}/glfw/lib/glfw3.lib
        ${THIRD_PARTY_DIR}/draco/lib/draco.lib
        cgltf
        stb_image
        rapidobj
        compat
        draco
)

set(SHADERC_DIR "" CACHE PATH "bgfx shaderc build output dir")
set(SHADERC_INCLUDE_DIR "" CACHE PATH "bgfx source dir containing bgfx_shader.sh")

find_program(SHADERC_EXECUTABLE shadercDebug HINTS
    "${SHADERC_DIR}"
    PATH_SUFFIXES win64_vs2022/bin/Shaderc
    NO_DEFAULT_PATH
)

if(SHADERC_EXECUTABLE AND NOT SHADERC_INCLUDE_DIR)
    message(FATAL_ERROR "SHADERC_INCLUDE_DIR must point at the bgfx source dir (contains bgfx_shader.sh) to rebuild shaders.")
endif()

if(SHADERC_EXECUTABLE)
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/vs_default.bin"
        COMMAND "${SHADERC_EXECUTABLE}" -f "${SHADER_OUTPUT_DIR}/vs_default.sc" -o "${SHADER_OUTPUT_DIR}/vs_default.bin" --type vertex --platform windows -p s_5_0 --varyingdef "${SHADER_OUTPUT_DIR}/varying.def.sc" -i "${SHADERC_INCLUDE_DIR}"
        DEPENDS "${SHADER_OUTPUT_DIR}/vs_default.sc" "${SHADER_OUTPUT_DIR}/varying.def.sc"
        COMMENT "Compiling vs_default.sc"
    )

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/fs_default.bin"
        COMMAND "${SHADERC_EXECUTABLE}" -f "${SHADER_OUTPUT_DIR}/fs_default.sc" -o "${SHADER_OUTPUT_DIR}/fs_default.bin" --type fragment --platform windows -p s_5_0 --varyingdef "${SHADER_OUTPUT_DIR}/varying.def.sc" -i "${SHADERC_INCLUDE_DIR}"
        DEPENDS "${SHADER_OUTPUT_DIR}/fs_default.sc" "${SHADER_OUTPUT_DIR}/varying.def.sc"
        COMMENT "Compiling fs_default.sc"
    )

    add_custom_target(CryonixShaders
        DEPENDS "${SHADER_OUTPUT_DIR}/vs_default.bin" "${SHADER_OUTPUT_DIR}/fs_default.bin"
    )
endif()

message(STATUS "Cryonix Target Platform: ${PLATFORM}")
