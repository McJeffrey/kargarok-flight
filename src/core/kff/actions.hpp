// Frei belegbare Flugtasten (Kargarok Flight). Die Module lesen ihre Taste ueber kff::act(...) statt fest ueber
// PAD_BUTTON_*. Die Zuordnung Aktion -> GameCube-Taste steht in den Konfig-Variablen bind_* (core/config.cpp); die
// Tabelle kBindable legt fest, welche Tasten es gibt. Ohne Aenderung gilt die Vorgabe, also das bisherige Verhalten.
//
// Zusammenhang (E-2): Dusklight ordnet jeden Controller (Xbox, PlayStation, GameCube) und die Tastatur den
// GameCube-Tasten zu. Die Mod ordnet den GameCube-Tasten die Flugaktionen zu.
#pragma once

#include "core/kff/input.hpp"

#include "m_Do/m_Do_controller_pad.h"

#include <cstddef>
#include <cstdint>

namespace kff {

// Flugaktionen, die die Module lesen. Der Ruf steht nicht hier: er wird im Pad-Hook entschieden (core/input.cpp).
enum class Act : uint8_t {
    Dismount,
    Dash,
    GearUp,
    GearDown,
    Brake,
    BarrelRoll,
    CameraBehind,
    CameraCloser,
    CameraFarther,
    Faster,   // bind_schneller (Vorgabe R), Gas halten
    Slower,   // bind_langsamer (Vorgabe L), Gas halten bis zum Schweben
    Count
};

// Belegte Taste einer Aktion nach dem letzten Pad-Tick. bit == 0: unbelegt, dann ist beides false.
struct ActRef {
    uint32_t bit = 0;
    bool pressed() const;  // Flanke dieses Pad-Ticks (Schnappschuss vor dem Konsum)
    bool held() const;     // gehalten
};

// Billig: liest nur die Belegung des letzten Pad-Ticks.
ActRef act(Act a);

// Belegbare GameCube-Tasten. Index = gespeicherter Konfigwert (bind_*), Index 0 = "Not set". Die Reihenfolge ist fest,
// neue Eintraege werden nur angehaengt.
struct BindableButton {
    const char* label;
    uint32_t bit;
};

inline constexpr BindableButton kBindable[] = {
    {"Not set", 0u},
    {"A", static_cast<uint32_t>(PAD_BUTTON_A)},
    {"B", static_cast<uint32_t>(PAD_BUTTON_B)},
    {"X", static_cast<uint32_t>(PAD_BUTTON_X)},
    {"Y", static_cast<uint32_t>(PAD_BUTTON_Y)},
    {"Z", static_cast<uint32_t>(PAD_TRIGGER_Z)},
    {"L", static_cast<uint32_t>(PAD_TRIGGER_L)},
    {"R", static_cast<uint32_t>(PAD_TRIGGER_R)},
    {"D-Pad Up", static_cast<uint32_t>(PAD_BUTTON_UP)},
    {"D-Pad Down", static_cast<uint32_t>(PAD_BUTTON_DOWN)},
    {"D-Pad Left", static_cast<uint32_t>(PAD_BUTTON_LEFT)},
    {"D-Pad Right", static_cast<uint32_t>(PAD_BUTTON_RIGHT)},
};
inline constexpr size_t kBindableCount = sizeof(kBindable) / sizeof(kBindable[0]);

// K11: Knopf-Bit je Anzeigefeld der Flugknoepfe, in der Reihenfolge Do (A), AStatus (B), X, Y, Z, R. Die Spielanzeige im
// Ritt (flug.cpp, label) liest sie. L und das Steuerkreuz haben kein Feld.
inline constexpr uint32_t kHudFieldBits[6] = {
    static_cast<uint32_t>(PAD_BUTTON_A), static_cast<uint32_t>(PAD_BUTTON_B), static_cast<uint32_t>(PAD_BUTTON_X),
    static_cast<uint32_t>(PAD_BUTTON_Y), static_cast<uint32_t>(PAD_TRIGGER_Z), static_cast<uint32_t>(PAD_TRIGGER_R)};

constexpr uint32_t bindableMask() {
    uint32_t m = 0;
    for (const auto& b : kBindable) {
        m |= b.bit;
    }
    return m;
}

// Die belegbaren Bits sind genau die Konsum-Maske des Rittes (kRideConsumedButtons). Start fehlt absichtlich.
static_assert(kBindableCount == 12, "kBindable: 0 = Not set plus 11 GameCube buttons");
static_assert(bindableMask() == kRideConsumedButtons, "bindable set must match the ride mask");

}  // namespace kff
