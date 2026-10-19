cmake_minimum_required(VERSION 3.25)

if (NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
endif ()

# Version des Mods. Gleich halten mit mod.json "version" und src/core/kff/version.hpp (KFF_MOD_VERSION).
# paket.sh prueft alle drei Stellen.
project(kargarok_flight VERSION 2.4.1 LANGUAGES CXX)

# Standalone build. Without options, cmake/FetchDusklight.cmake (from TwilitRealm/mod-template) fetches the pinned
# Dusklight sources into ./dusklight:
#   cmake -S . -B build
# Use an existing checkout instead:
#   cmake -S . -B build -DDUSKLIGHT_DIR=<dusklight>
# DUSK_GAME_EXE avoids the link-stub download in cmake/ModSDK.cmake: a game binary or link stub (Windows: a .lib).
# Anleitung fuer Windows: WINDOWS.md. Der Build fuer alle Plattformen laeuft in .github/workflows/build.yml.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Dusklight-Version, gegen die der Mod gebaut und getestet ist (Tag oder Commit-SHA).
    if (NOT DUSKLIGHT_VERSION)
        set(DUSKLIGHT_VERSION "v2.0.3")
    endif ()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # MSVC meldet __cplusplus sonst als 199711L; dolphin/types.h definiert dann nullptr als NULL und die STL bricht.
    # Dusklight setzt das Flag nur im eigenen Top-Level-CMakeLists, nicht im SDK.
    if (MSVC)
        add_compile_options(/Zc:__cplusplus)
    endif ()
    add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
endif ()

# KFF_DEV_AUTOTEST=ON: Testbuild mit Autotest (nur Entwicklung, KFF_AUTOTEST=<name>). Die Compile-Definition
# KFF_DEV_AUTOTEST=1 gilt nur dann. Standard OFF (Release): src/core/autotest.cpp wird nicht gebaut, die Schnittstelle
# im Header bleibt als leere Stubs erhalten (Entscheidung D11).
option(KFF_DEV_AUTOTEST "Autotest nur Entwicklung" OFF)

set(KFF_SOURCES
    src/core/services.cpp
    src/core/core.cpp
    src/core/log.cpp
    src/core/region.cpp
    src/core/config.cpp
    src/core/panel.cpp
    src/core/actions.cpp
    src/core/steuerung.cpp
    src/core/session.cpp
    src/core/fakepath.cpp
    src/core/hooks.cpp
    src/core/ride.cpp
    src/core/summon.cpp
    src/core/input.cpp
    src/core/modules.cpp
    src/modules/flug.cpp
    src/modules/freiheit.cpp
    src/modules/griff.cpp
    src/modules/tempo.cpp
    src/modules/kamera.cpp
    src/modules/autowolf.cpp
    src/modules/mensch.cpp
)
if (KFF_DEV_AUTOTEST)
    list(APPEND KFF_SOURCES src/core/autotest.cpp src/core/input_test.cpp src/core/panel_test.cpp
        src/core/actions_test.cpp src/core/steuerung_test.cpp src/core/zustand_test.cpp)
endif ()

# Paket (.dusk): mod.json, lib/<Plattform>/mod.so (macOS) bzw. mod.dll (Windows) und alles unter res/ (icon.png,
# banner.png). Der Plattformordner (z. B. macos-arm64 oder windows-amd64) kommt aus ModSDK.cmake (_mod_lib_info).
# RES_DIR packt den ganzen Ordner. docs/ (Bilder und Screenshots) liegt absichtlich ausserhalb von res/ und kommt nicht ins Paket.
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.png" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/banner.png")
    message(WARNING "res/icon.png oder res/banner.png fehlt: das Paket bekommt dann kein Icon bzw. Banner.")
endif ()

add_mod(kargarok_flight
    FEATURES game
    SOURCES ${KFF_SOURCES}
    MOD_JSON mod.json
    RES_DIR  res
)

if (KFF_DEV_AUTOTEST)
    target_compile_definitions(kargarok_flight PRIVATE KFF_DEV_AUTOTEST=1)
endif ()

# Includes wie "core/kff/session.hpp" relativ zu src/.
target_include_directories(kargarok_flight PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# macOS: ModSDK strippt nur unter Linux (ModSDK.cmake:325-328). Ohne -S enthaelt mod.so Debug-Stabs mit
# Entwicklerpfaden des Build-Rechners. -S entfernt nur die Stabs; die Export-Symbole bleiben.
if (APPLE AND CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    add_custom_command(TARGET kargarok_flight POST_BUILD
        COMMAND "${CMAKE_STRIP}" -S $<TARGET_FILE:kargarok_flight>)
endif ()
