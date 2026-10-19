// Konfiguration: nur wirksame Schluessel werden beim ConfigService registriert (Entscheidung D10). Jeder Wert wird
// beim Lesen auf seinen gueltigen Bereich geklemmt; ungueltige Werte aus config.json wirken also nie.
// Ausnahme (E-10, strict): die Flugtasten bind_* fallen bei einem ungueltigen Wert auf die Vorgabe zurueck, nicht auf
// den Klemmwert. Die Warnung kommt einmal je Schluessel (checkStoredValues).
// cam_mode hat keine Wirkung mehr: der Schluessel wird nicht registriert, Key::CamMode liefert immer den Standard 0.
// invert_steer ist der Altlast-Alias von lenkung_invertieren (vor v2.1.0): er wirkt zusaetzlich, wenn er true ist.
#include "core/kff/config.hpp"

#include "core/actions.hpp"
#include "core/kff/log.hpp"
#include "core/kff/services.hpp"

#include <cstddef>

namespace kff::config {
namespace {

struct VarDef {
    const char* name;    // nullptr: Schluessel ohne Wirkung, wird nicht registriert (nur Standardwert)
    ConfigVarType type;
    bool defBool;
    int64_t defInt;
    double defFloat;
    int64_t minInt;      // Klemmbereich (Ganzzahl und Gleitkomma)
    int64_t maxInt;
    double minFloat;
    double maxFloat;
    bool strict;         // true: ungueltiger Wert -> Standard statt Klemmwert (bind_*, E-10)
};

// Reihenfolge wie Key. Namen: nur [A-Za-z0-9_-].
constexpr VarDef kDefs[] = {
    {nullptr, CONFIG_VAR_INT, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // Mode entfaellt (D9, K8): nur frei
    {"lenkung_invertieren", CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // InvertSteer (D10)
    {nullptr, CONFIG_VAR_INT, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // CamMode: cam_mode entfallen (D10)
    {"cruise_speed", CONFIG_VAR_FLOAT, false, 0, 50.0, 0, 0, 0.0, 200.0, false},  // CruiseSpeed 0..200
    {nullptr, CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // event_keep entfaellt (K8): ohne Wirkung
    {nullptr, CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // roof_limit entfaellt (D9): keine Decke
    {"min_ground_clearance", CONFIG_VAR_FLOAT, false, 0, -1.0, 0, 0, -1.0, 2000.0, false},  // -1..2000
    {"start_gang", CONFIG_VAR_INT, false, 3, 0.0, 1, 5, 0.0, 0.0, false},  // StartGear 1..5, Standard 3
    {"hud_anzeigen", CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // HudShow, N6: Standard AUS
    {"sichtfeld_effekt", CONFIG_VAR_BOOL, true, 0, 0.0, 0, 0, 0.0, 0.0, false},  // FovEffect
    {"mensch_ritt", CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // MenschRitt, Standard AUS (D4)
    {"decke_abstieg", CONFIG_VAR_BOOL, false, 0, 0.0, 0, 0, 0.0, 0.0, false},  // DeckeAbstieg, Standard AUS (E10)
    {"kamera_abstand", CONFIG_VAR_FLOAT, false, 0, 650.0, 0, 0, 300.0, 1400.0, false},  // KameraAbstand (Wolf)
    {"kamera_abstand_mensch", CONFIG_VAR_FLOAT, false, 0, 1100.0, 0, 0, 300.0, 2000.0, false},  // KameraAbstandMensch
    // Flugtasten (E-10): Index in kff::kBindable (0 = Not set, 11 = hoechster Index), strict. Vorgaben = heute (E-11).
    {"bind_ruf_halten", CONFIG_VAR_INT, false, 7, 0.0, 0, 11, 0.0, 0.0, true},      // CallHold: R
    {"bind_ruf_tipp", CONFIG_VAR_INT, false, 2, 0.0, 0, 11, 0.0, 0.0, true},        // CallTap: B
    {"bind_absteigen", CONFIG_VAR_INT, false, 2, 0.0, 0, 11, 0.0, 0.0, true},       // Dismount: B
    {"bind_dash", CONFIG_VAR_INT, false, 1, 0.0, 0, 11, 0.0, 0.0, true},            // Dash: A
    {"bind_gang_hoch", CONFIG_VAR_INT, false, 8, 0.0, 0, 11, 0.0, 0.0, true},       // GearUp: Steuerkreuz hoch
    {"bind_gang_runter", CONFIG_VAR_INT, false, 9, 0.0, 0, 11, 0.0, 0.0, true},     // GearDown: Steuerkreuz runter
    {"bind_bremse", CONFIG_VAR_INT, false, 5, 0.0, 0, 11, 0.0, 0.0, true},          // Brake: Z
    {"bind_fassrolle", CONFIG_VAR_INT, false, 4, 0.0, 0, 11, 0.0, 0.0, true},       // BarrelRoll: Y
    {"bind_kamera_hinten", CONFIG_VAR_INT, false, 3, 0.0, 0, 11, 0.0, 0.0, true},   // CameraBehind: X
    {"bind_kamera_naeher", CONFIG_VAR_INT, false, 11, 0.0, 0, 11, 0.0, 0.0, true},  // CameraCloser: Steuerkreuz rechts
    {"bind_kamera_weiter", CONFIG_VAR_INT, false, 10, 0.0, 0, 11, 0.0, 0.0, true},  // CameraFarther: Steuerkreuz links
    {"bind_schneller", CONFIG_VAR_INT, false, 7, 0.0, 0, 11, 0.0, 0.0, true},       // Faster: R (halten, stufenlos schneller)
    {"bind_langsamer", CONFIG_VAR_INT, false, 6, 0.0, 0, 11, 0.0, 0.0, true},       // Slower: L (halten, bis zum Schweben)
    // Kurztipps bei den ersten Ritten (Version 2.3.0, neu). Ohne gespeicherten Wert gilt die Vorgabe.
    {"tipps_anzeigen", CONFIG_VAR_BOOL, true, 0, 0.0, 0, 0, 0.0, 0.0, false},  // TipsShow, Standard AN
    {"tipps_rest", CONFIG_VAR_INT, false, 3, 0.0, 0, 3, 0.0, 0.0, false},      // TipsRest 0..3, Standard 3
};

constexpr size_t kCount = static_cast<size_t>(Key::Count);
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == kCount, "config table and Key must match");

// Altlast-Alias fuer die Nick-Umkehr (vor v2.1.0). Wirkt als Oder zu lenkung_invertieren.
constexpr const char* kLegacyInvertName = "invert_steer";

ConfigVarHandle g_handles[kCount] = {};
ConfigVarHandle g_legacyInvertHandle = 0;
bool g_registered = false;

size_t idx(Key key) {
    return static_cast<size_t>(key);
}

int64_t clampInt(int64_t v, int64_t lo, int64_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

double clampFloat(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Wirksamer Gleitkommawert: nicht endliche Werte (NaN, inf, betraglich ueber 1e300) fallen auf den Standard zurueck,
// danach wird geklemmt. Gleiche Regel fuer getFloat und die Pruefung nach der Registrierung.
double effectiveFloat(size_t i, double value) {
    if (value != value || value > 1.0e300 || value < -1.0e300) {
        value = kDefs[i].defFloat;
    }
    return clampFloat(value, kDefs[i].minFloat, kDefs[i].maxFloat);
}

// Wirksamer Ganzzahlwert: strict faellt auf den Standard zurueck, sonst wird geklemmt.
int64_t effectiveInt(size_t i, int64_t value) {
    const VarDef& d = kDefs[i];
    if (d.strict) {
        return (value < d.minInt || value > d.maxInt) ? d.defInt : value;
    }
    return clampInt(value, d.minInt, d.maxInt);
}

// Einmalige Pruefung nach der Registrierung (nicht im Spielraum). Jeder Schluessel mit einem Wert ausserhalb des
// Bereichs oder mit einem nicht endlichen Wert bekommt genau eine Warnzeile; die Lesefunktionen wirken danach wie oben.
// Falsche Typen meldet der Host selbst (Ladefehler mit dem vollen Schluesselnamen, Standard wirksam), dazu gibt
// es hier keine zweite Zeile. So entspricht die Zahl der Warnzeilen der Zahl ungueltiger Schluessel.
void checkStoredValues() {
    for (size_t i = 0; i < kCount; ++i) {
        const VarDef& d = kDefs[i];
        if (d.name == nullptr || g_handles[i] == 0) {
            continue;
        }
        if (d.type == CONFIG_VAR_INT) {
            int64_t raw = d.defInt;
            if (svc_config->get_int(mod_ctx, g_handles[i], &raw) != MOD_OK) {
                continue;
            }
            if (raw < d.minInt || raw > d.maxInt) {
                if (d.strict) {
                    logWarn("config %s = %lld ausserhalb %lld bis %lld, Vorgabe %lld wirksam", d.name,
                        static_cast<long long>(raw), static_cast<long long>(d.minInt),
                        static_cast<long long>(d.maxInt), static_cast<long long>(d.defInt));
                } else {
                    logWarn("config %s = %lld ausserhalb %lld bis %lld, wirksam %lld", d.name,
                        static_cast<long long>(raw), static_cast<long long>(d.minInt),
                        static_cast<long long>(d.maxInt),
                        static_cast<long long>(clampInt(raw, d.minInt, d.maxInt)));
                }
            }
        } else if (d.type == CONFIG_VAR_FLOAT) {
            double raw = d.defFloat;
            if (svc_config->get_float(mod_ctx, g_handles[i], &raw) != MOD_OK) {
                continue;
            }
            const bool finite = raw == raw && raw <= 1.0e300 && raw >= -1.0e300;
            if (!finite || raw < d.minFloat || raw > d.maxFloat) {
                logWarn("config %s = %g ungueltig (Bereich %g bis %g), wirksam %g", d.name, raw,
                    d.minFloat, d.maxFloat, effectiveFloat(i, raw));
            }
        }
    }
}

// Eine Zeile mit den wirksamen Werten (nach dem Klemmen), damit das Log die Konfiguration belegt.
void logEffectiveValues() {
    logInfo("config wirksam: lenkung_invertieren=%d start_gang=%lld cruise_speed=%g min_ground_clearance=%g "
            "hud_anzeigen=%d sichtfeld_effekt=%d mensch_ritt=%d decke_abstieg=%d",
        getBool(Key::InvertSteer) ? 1 : 0, static_cast<long long>(getInt(Key::StartGear)),
        getFloat(Key::CruiseSpeed), getFloat(Key::MinGroundClearance), getBool(Key::HudShow) ? 1 : 0,
        getBool(Key::FovEffect) ? 1 : 0, getBool(Key::MenschRitt) ? 1 : 0, getBool(Key::DeckeAbstieg) ? 1 : 0);
}

}  // namespace

void registerVars() {
    if (svc_config == nullptr) {
        logWarn("config service not available, default values active");
        actions::load();  // Belegung mit der Vorgabe, genau eine Zeile (E-10)
        return;
    }
    for (size_t i = 0; i < kCount; ++i) {
        if (kDefs[i].name == nullptr) {
            continue;  // ohne Wirkung (cam_mode): nicht registriert, Standardwert
        }
        ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
        desc.name = kDefs[i].name;
        desc.type = kDefs[i].type;
        desc.default_bool = kDefs[i].defBool;
        desc.default_int = kDefs[i].defInt;
        desc.default_float = kDefs[i].defFloat;
        ConfigVarHandle handle = 0;
        const ModResult r = svc_config->register_var(mod_ctx, &desc, &handle);
        if (r != MOD_OK) {
            logWarn("config var %s not registered (%d), default active", kDefs[i].name, static_cast<int>(r));
            continue;
        }
        g_handles[i] = handle;
    }
    ConfigVarDesc legacy = CONFIG_VAR_DESC_INIT;
    legacy.name = kLegacyInvertName;
    legacy.type = CONFIG_VAR_BOOL;
    legacy.default_bool = false;
    if (svc_config->register_var(mod_ctx, &legacy, &g_legacyInvertHandle) != MOD_OK) {
        logWarn("config alias %s not registered, alias inactive", kLegacyInvertName);
        g_legacyInvertHandle = 0;
    }
    g_registered = true;
    checkStoredValues();
    logEffectiveValues();
    actions::load();  // Belegung der Flugtasten: genau eine Zeile im Log, auch im Release (E-10)
}

uint64_t handleOf(Key key) {
    return g_registered ? g_handles[idx(key)] : 0;
}

bool getBool(Key key) {
    const size_t i = idx(key);
    bool value = kDefs[i].defBool;
    if (svc_config != nullptr && g_registered && g_handles[i] != 0) {
        (void)svc_config->get_bool(mod_ctx, g_handles[i], &value);
    }
    if (key == Key::InvertSteer && svc_config != nullptr && g_registered && g_legacyInvertHandle != 0) {
        bool legacy = false;
        if (svc_config->get_bool(mod_ctx, g_legacyInvertHandle, &legacy) == MOD_OK && legacy) {
            value = true;  // Altlast invert_steer = true wirkt zusaetzlich
        }
    }
    return value;
}

int64_t getInt(Key key) {
    const size_t i = idx(key);
    int64_t value = kDefs[i].defInt;
    if (svc_config != nullptr && g_registered && g_handles[i] != 0) {
        (void)svc_config->get_int(mod_ctx, g_handles[i], &value);
    }
    return effectiveInt(i, value);
}

double getFloat(Key key) {
    const size_t i = idx(key);
    double value = kDefs[i].defFloat;
    if (svc_config != nullptr && g_registered && g_handles[i] != 0) {
        (void)svc_config->get_float(mod_ctx, g_handles[i], &value);
    }
    return effectiveFloat(i, value);  // nicht endlich: Standard, danach geklemmt
}

bool setBool(Key key, bool value) {
    const size_t i = idx(key);
    if (svc_config == nullptr || !g_registered || g_handles[i] == 0) {
        return false;
    }
    return svc_config->set_bool(mod_ctx, g_handles[i], value) == MOD_OK;
}

bool setInt(Key key, int64_t value) {
    const size_t i = idx(key);
    if (svc_config == nullptr || !g_registered || g_handles[i] == 0) {
        return false;
    }
    return svc_config->set_int(mod_ctx, g_handles[i], value) == MOD_OK;
}

}  // namespace kff::config
