// Eingabe-Schnappschuss des Kerns (Port 1), genommen im Post-Hook von mDoCPd_c::read VOR dem Konsum.
//
// Im Ritt entzieht der Kern der Spiellogik A, B, X, Y, Z, L, R und das Steuerkreuz (Maskenkonstante unten).
// Danach liefert mDoCPd_c::getTrig*/getHold* fuer diese Tasten 0. Module, die eine dieser Tasten brauchen
// (Fassrolle Y, Dash A, Kamera Steuerkreuz), lesen sie hier:
//
//   if (kff::input().isPressed(PAD_BUTTON_Y)) { ... }     // Flanke dieses Ticks
//   if (kff::input().isHeld(PAD_TRIGGER_R)) { ... }       // gehalten
//
// Stick und C-Stick sind nicht betroffen: sie stehen auch im Schnappschuss und in mDoCPd_c unveraendert.
// Start bleibt immer frei (Pause, Menue).
#pragma once

#include "m_Do/m_Do_controller_pad.h"

#include <cstdint>

namespace kff {

// Die Bits, die der Kern im Ritt aus der Spiellogik entfernt. Start ist nicht dabei.
constexpr uint32_t kRideConsumedButtons = static_cast<uint32_t>(PAD_BUTTON_A) | static_cast<uint32_t>(PAD_BUTTON_B) |
                                          static_cast<uint32_t>(PAD_BUTTON_X) | static_cast<uint32_t>(PAD_BUTTON_Y) |
                                          static_cast<uint32_t>(PAD_TRIGGER_Z) | static_cast<uint32_t>(PAD_TRIGGER_L) |
                                          static_cast<uint32_t>(PAD_TRIGGER_R) | static_cast<uint32_t>(PAD_BUTTON_UP) |
                                          static_cast<uint32_t>(PAD_BUTTON_DOWN) | static_cast<uint32_t>(PAD_BUTTON_LEFT) |
                                          static_cast<uint32_t>(PAD_BUTTON_RIGHT);

struct Input {
    uint32_t frame = 0;      // Zaehler der Kern-Ticks (je mDoCPd_c::read)
    uint32_t held = 0;       // mButtonFlags vor dem Konsum: Tasten gehalten
    uint32_t pressed = 0;    // mPressedButtonFlags vor dem Konsum: Tastenflanken dieses Ticks
    float stickX = 0.0f;     // linker Stick (unveraendert)
    float stickY = 0.0f;
    float cStickX = 0.0f;    // C-Stick (unveraendert)
    float cStickY = 0.0f;
    float analogA = 0.0f;    // Analogwerte vor dem Konsum
    float analogB = 0.0f;
    float triggerL = 0.0f;
    float triggerR = 0.0f;
    // WP3, einmal je Tick vom Kern gesetzt: uiBlocked = Pause, Menue, Karte, Item-Rad, Dialog, Ereignis, Szenenwechsel oder
    // Dusklight-Dokument offen (E5, der Kern verbraucht dann nichts). uiDocument = ein Dusklight-Dokument ist sichtbar: das
    // Spiel bekommt keine Pad-Eingabe, laeuft aber weiter (ui_halt im Modul flug).
    bool uiBlocked = false;
    bool uiDocument = false;

    bool isHeld(uint32_t bits) const { return (held & bits) != 0; }
    bool isPressed(uint32_t bits) const { return (pressed & bits) != 0; }
};

// Schnappschuss des letzten Ticks. Nach der Injektion des Autotests, vor dem Konsum.
const Input& input();

}  // namespace kff
