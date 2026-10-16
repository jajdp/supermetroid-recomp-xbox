# Target UWP (Xbox Series, modo desarrollador) de SuperMetroidRecomp.
#
# Aquí NO se reescribe el host: se reusa el `src/main.c` del juego y solo se cambian tres
# cosas, todas bajo el define SM_UWP:
#   - el presentador dibuja con Direct3D 11 sobre la CoreWindow, no con OpenGL, que la
#     Xbox no permite en UWP, ni con el renderizador de SDL, que pide un HWND;
#   - la entrada es `uwp_entry.cpp` (WinMain -> SDL_RunApp -> el main del juego), porque en
#     UWP el proceso arranca por CoreApplication y no por main();
#   - el volcado post mortem va sin dbghelp/tlhelp32, que no existen en AppContainer.
# Los parches los aplica `tools/apply_uwp.py`; la compilación de escritorio no cambia.
#
# SDL3 tiene que ser uno compilado para UWP (bit APPCONTAINER, CRT «_app» y WinRT, sin
# user32): el de escritorio no sirve. Ver docs/DEPENDENCIES.md.
#
# Configurar (fuera del árbol, y con ruta corta por MAX_PATH), con el entorno
# `vcvarsall.bat x64 store` ya cargado en la ventana:
#   cmake -S <este dir> -B <build> -G Ninja ^
#     -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
#     -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
#     -DSM_SRC=<ruta a tu copia del juego, CON BARRAS NORMALES> ^
#     -DSDL3_UWP=<ruta al SDL3 de UWP, CON BARRAS NORMALES>
#   cmake --build <build> --target sm_uwp
cmake_minimum_required(VERSION 3.20)
project(sm_uwp C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED SM_SRC)
    message(FATAL_ERROR "Pasa -DSM_SRC=<ruta al clon smrepo>")
endif()
if(NOT DEFINED SDL3_UWP)
    message(FATAL_ERROR "Pasa -DSDL3_UWP=<ruta a uwp-dep-sdl/x64>")
endif()

set(SNESRECOMP_ROOT ${SM_SRC}/snesrecomp)
set(SNESRECOMP_SDL_BACKEND "SDL3" CACHE STRING "" FORCE)
include(${SNESRECOMP_ROOT}/runner/runner.cmake)

file(GLOB SM_GEN CONFIGURE_DEPENDS "${SM_SRC}/src/gen/*.c")
if(NOT SM_GEN)
    message(FATAL_ERROR "No hay fuentes generadas en ${SM_SRC}/src/gen "
                        "(corre tools/regen.sh con SNESRECOMP_ANALYSIS_BACKEND=native, "
                        "que es el valor por defecto). NO uses el backend python: emite "
                        "el doble de variantes AOT, una esta mal generada y deja el juego "
                        "a 4 fps. Ver notas-xbox-sdl3-d3d11.md.")
endif()

# El runtime específico del juego. Mismo listado que SM_HOST_SOURCES del repo, SIN el
# presentador de escritorio (opengl.c, glsl_shader.c, third_party/gl_core).
set(SM_RUNTIME
    ${SM_SRC}/src/config.c
    ${SM_SRC}/src/sm_display.c
    ${SM_SRC}/src/main.c
    ${SM_SRC}/src/post_mortem.c
    ${SM_SRC}/src/gen_stubs.c
    ${SM_SRC}/src/sm_cpu_infra.c
    ${SM_SRC}/src/sm_rtl.c
    ${SM_SRC}/src/sm_spc_player.c
    ${SM_SRC}/src/sm_renderer.c
    ${SM_SRC}/src/sm_mode7.c
    ${SM_SRC}/src/sm_video.c
    ${SM_SRC}/src/sm_mods.c
    ${SM_SRC}/src/fiber_compat.c)

add_executable(sm_uwp WIN32
    ${SNESRECOMP_RUNNER_SOURCES}
    ${SM_RUNTIME}
    ${SM_GEN}
    ${CMAKE_CURRENT_SOURCE_DIR}/uwp_entry.cpp)

target_include_directories(sm_uwp PRIVATE
    ${SM_SRC}
    ${SM_SRC}/src
    ${SM_SRC}/recomp
    # sm_mods.h incluye recomp_launcher.h aunque no se compile el lanzador (solo usa sus
    # tipos), asi que hace falta la cabecera de recomp-ui; la biblioteca NO se enlaza.
    ${SM_SRC}/recomp-ui/src
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    ${SDL3_UWP}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(sm_uwp PRIVATE
    SM_UWP=1
    SNESRECOMP_SDL3=1
    LNG_SDL3=1
    SDL_MAIN_HANDLED
    SMW_DISABLE_LM=1
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
    _CRT_SECURE_NO_WARNINGS)

target_compile_options(sm_uwp PRIVATE
    $<$<COMPILE_LANGUAGE:C>:/W0>
    $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    $<$<COMPILE_LANGUAGE:CXX>:/bigobj>)

# CRT «store» (lo trae el entorno vcvarsall x64 store + /MD) + OneCore, sin user32 ni
# OpenGL. SDL3 es el .lib del build UWP; su .dll va dentro del paquete.
target_link_libraries(sm_uwp PRIVATE
    ${SNESRECOMP_RUNNER_LIBRARIES}
    ${SDL3_UWP}/lib/SDL3.lib
    d3d11 dxgi WindowsApp OneCore)

# El generador Ninja de WindowsStore NO añade /APPCONTAINER solo; sin ese bit el loader de
# UWP rechaza el paquete al activarlo.
target_link_options(sm_uwp PRIVATE /APPCONTAINER)

set_target_properties(sm_uwp PROPERTIES OUTPUT_NAME SuperMetroidRecomp)
