cmake_minimum_required(VERSION 3.20)
project(CraftEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT so the release .exe runs without a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(CraftEngine WIN32
    src/main.cpp
    src/chunk.cpp
    src/world.cpp
    src/renderer.cpp
)

target_include_directories(CraftEngine PRIVATE src)

if (MSVC)
    target_compile_options(CraftEngine PRIVATE
        /W3 /O2 /fp:fast /arch:SSE2 /permissive- /Zc:__cplusplus
        $<$<CONFIG:Debug>:/Od /Zi>
    )
    target_compile_definitions(CraftEngine PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_link_libraries(CraftEngine PRIVATE d3d10 dxgi d3dcompiler)

if (MSVC)
    set_target_properties(CraftEngine PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /OPT:REF /OPT:ICF"
    )
endif()

install(TARGETS CraftEngine RUNTIME DESTINATION .)
