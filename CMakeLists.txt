cmake_minimum_required(VERSION 3.20)
project(CraftEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT so the release .exe runs without a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# zlib (PNG/ZIP inflate) — fetched source, built static, no system dependency.
include(FetchContent)
FetchContent_Declare(zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
)
FetchContent_MakeAvailable(zlib)

add_executable(CraftEngine WIN32
    src/main.cpp
    src/chunk.cpp
    src/world.cpp
    src/renderer.cpp
    src/textures.cpp
    src/zip.cpp
    src/png.cpp
    src/assets.cpp
    src/vanilla_data.cpp
    src/rng.cpp
    src/gui.cpp
    src/font.cpp
    src/sound.cpp
    src/player.cpp
    src/inventory.cpp
    src/combat.cpp
    src/mobs.cpp
    src/achievements.cpp
)

target_include_directories(CraftEngine PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})

if (MSVC)
    target_compile_options(CraftEngine PRIVATE
        /W3 /O2 /fp:precise /arch:SSE2 /permissive- /Zc:__cplusplus
        $<$<CONFIG:Debug>:/Od /Zi>
    )
    target_compile_definitions(CraftEngine PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_link_libraries(CraftEngine PRIVATE d3d10 dxgi d3dcompiler zlibstatic winmm)

if (MSVC)
    set_target_properties(CraftEngine PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /OPT:REF /OPT:ICF"
    )
endif()

install(TARGETS CraftEngine RUNTIME DESTINATION .)