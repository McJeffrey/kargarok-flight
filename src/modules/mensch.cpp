// Modul "mensch": Ritt als MENSCH-Link am freundlichen Kargarok (Entscheidung D4: Standard AUS).
//
// Mit Config mensch_ritt = true nimmt dieses Modul den Ruf als Mensch an (onHumanCall) und Link reitet als Mensch.
// Mit AUS lehnt es ab; dann verwandelt das Modul autowolf wie bisher (Modulreihenfolge: mensch vor autowolf).
//
// Mechanik (Variante B aus kff-human-huhn2-b-echtes-huhn, hier im Kern verdrahtet):
//   - Kein Cargo. Link haelt den Vogel ueber den nativen Greifslot: eine unsichtbare Huhn-Instanz (fpcNm_NI_e) wird per
//     setGrabItemActor + setGrabUpperAnime(-1) eingehaengt, procAutoJumpInit(1) startet den nativen Gleit-Proc.
//     Die Sitzung traegt dafuer rideGrip = true (kein setPlayerRideOn, kein Abbruch durch fehlendes Cargo).
//   - Haengepose (Prototyp krallen-greifen-r5): Die Handmitte liegt auf der Griffmitte (Mitte der Vogelknoechel, plus
//     kGripAbove). Die Faeuste liegen je kHandHalfSep seitlich dieser Mitte, also im Handabstand und nicht im Knoechelabstand.
//   - Pendel: Schraeglage (zeta kSwingZeta) folgt der halben Vogelschraeglage, der Nick ist kritisch gedaempft und folgt der
//     Vorwaertsbeschleunigung. Link wird im posMove (Post) und vor dem Aufbau der Matrix (setMatrix, Pre) auf den Griff gepinnt.
//   - Vogel-Seite (daKago_c::execute, Post): Beine einfrieren, Fuesse und Krallen (Zehen, Krallenwurzel) ausrichten, danach
//     Repin von Link auf den Griff dieses Ticks.
//   - Faeuste: Handformen 0 (links) und 6 (rechts, geschlossen), Handgelenk-Roll +90/-90 Grad (spiegelsymmetrisch),
//     Ellbogen beider Arme gestreckt (0 und 0).
//   - Loslassen nur per B: daAlink_c::freeGrabItem (Pre) wird im Ritt blockiert, ausser beim eigenen Ritt-Ende.
//   - Ritt-Ende (onRideEnd, vom Kern VOR dem Loesen von Link gerufen): Griff loesen, Huhn-Instanz entfernen.
//   - Griff-Waechter (onFrameLink, vor daAlink_c::execute): haelt Greifslot, Traegerstatus, Gleit-Proc, Greifanimation.
//   - Keine eigenen PadRead-Hooks: Eingaben kommen aus dem Kern-Schnappschuss.
//
// GRIFF-ABSCHNITT: Haltung, Pendel, Handgeometrie und Vogelkrallen zwischen den Marken "GRIFF-ABSCHNITT BEGIN/END".
// Uebernommen aus krallen-greifen-r5 mit den Standardwerten des Prototyps (ohne Umgebungsschalter), dabei kSwingZeta 0.6
// nach mensch-geruest.md statt 0.9. Nicht uebernommen: Kopfblick (G5, nur im Prototyp), Sporen (im Standard ohne Wirkung),
// Sicherheitsboden, Hoehenhalt und Umkehr ueber Leere (Flugmodule, nicht Teil des Griffs).
//
// Logs: Zeilen "kff: mensch: ..." (Ruf, Griff, Ende). Messzeilen (Spur, Messung, Krallenwurzel) und das Inventar nur im
// Testbuild (KFF_DEV_AUTOTEST). Test-Szenario "mensch" nur im Testbuild (KFF_AUTOTEST=mensch).
#include "d/dolzel_rel.h"

#include "core/kff/kff.hpp"

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_mtx.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

// ---------------------------------------------------------------------------------------------
// Zustand des Moduls. Der Ritt selbst gehoert dem Kern (kff::Session); hier liegt nur der Griff.
// ---------------------------------------------------------------------------------------------

enum class Grip : uint8_t {
    Off,      // kein Griff
    Pending,  // Ritt laeuft, Huhn-Instanz angelegt, Griff noch nicht eingehaengt
    Holding,  // Link haelt den Vogel im Griff
};

constexpr uint32_t kAttachAfterFrames = 2;   // Huhn-Instanz erst nach zwei Link-Frames einhaengen (wie huhn2-b)
constexpr uint32_t kPendingMaxFrames = 120;  // ohne Griff nach so vielen Link-Frames geordnetes Ritt-Ende

Grip g_grip = Grip::Off;
fpc_ProcID g_niId = kff::kNoProc;  // unsichtbare Huhn-Instanz (fpcNm_NI_e) im Greifslot
uint32_t g_pendingFrames = 0;      // Link-Frames seit dem Ritt-Start ohne eingehaengten Griff
bool g_releaseAllowed = false;     // nur waehrend des eigenen Ritt-Endes true
uint32_t g_calls = 0;              // angenommene Rufe als Mensch
uint32_t g_starts = 0;             // eingehaengte Griffe
uint32_t g_ends = 0;               // beendete Griffe
uint32_t g_releaseBlocked = 0;     // blockierte Loslass-Versuche (freeGrabItem)
uint32_t g_repairGrab = 0;         // Griff-Waechter: Reparaturen je Pfad
uint32_t g_repairCarry = 0;
uint32_t g_repairProc = 0;
uint32_t g_repairUpper = 0;
uint32_t g_logExc = 0;             // gedrosselte Ausnahme-Logs
uint32_t g_logBlock = 0;           // gedrosselte Loslass-Logs

// ===== GRIFF-ABSCHNITT BEGIN (Haltung, Pendel, Handgeometrie, Vogelkrallen; austauschbar) =====================================

// Gelenke des sichtbaren Vogelmodells: Oberschenkel (Wurzel des eingefrorenen Teils), Knoechel, Zehen (Krallen) und die
// Krallenwurzel 0x23 (Anhaengung).
constexpr int kJntLegL1 = 0x15;
constexpr int kJntLegR1 = 0x1A;
constexpr int kJntLegL3 = 0x17;  // Knoechel links (LEG_3)
constexpr int kJntLegR3 = 0x1C;  // Knoechel rechts
constexpr int kJntToeLB = 0x18;  // hintere Zehe links
constexpr int kJntToeLF = 0x19;  // vordere Zehe links
constexpr int kJntToeRB = 0x1D;  // hintere Zehe rechts
constexpr int kJntToeRF = 0x1E;  // vordere Zehe rechts
constexpr int kJntClaw = 0x23;   // Krallenwurzel (Anhaengung, Kalibrierung ueber kAttachWindow Ticks)
constexpr int kMaxJoints = 64;

// Gelenke des Link-Modells fuer die Handgelenkdrehung (Ellbogen und Handgelenk).
constexpr int kLinkJntElbowL = 8;
constexpr int kLinkJntHandL = 9;
constexpr int kLinkJntElbowR = 13;
constexpr int kLinkJntHandR = 14;

// Handform "Faust" (Kandidat b aus kff-human-v2): links 0, rechts 6, geschlossene Haende.
constexpr uint8_t kFistLeft = 0;
constexpr uint8_t kFistRight = 6;

constexpr float kHandHalfSep = 21.75f;     // halber Handabstand (43.5): jede Faust liegt so weit seitlich der Knoechelmitte
constexpr float kGripAbove = 4.0f;         // Faeuste ueber den Knoechelgelenken (r2b)
constexpr float kHandsUpDefault = 167.0f;  // Wurzel-zu-Hand vor der ersten Messung (r2b, Link aufrecht)

// Pendel. Schraeglage: zeta 0.6 nach mensch-geruest.md (der Prototyp r5 hatte 0.9). Nick: kritisch gedaempft (r5, G6).
constexpr float kSwingOmega = 3.0f;   // rad/s
constexpr float kSwingZeta = 0.6f;
constexpr float kPitchZeta = 1.0f;
constexpr float kPitchGain = 0.8f;    // rad je (Einheit/Tick^2) der Vorwaertsbeschleunigung des Vogels
constexpr float kPitchMax = 0.35f;    // rad, Saettigung des Nicks
constexpr float kAccSmooth = 0.08f;   // Glaettung der Vorwaertsbeschleunigung je Tick (r5, G6)
constexpr float kBankScale = 0.5f;    // Anteil der Vogelschraeglage, dem die Schraeglage von Link folgt (r5)
constexpr float kSimDt = 1.0f / 30.0f;
constexpr float kRad16 = 6.2831853f / 65536.0f;  // Radiant je s16-Einheit

// Krallen (r2b, r5 G3): die Spitze liegt kClawTip ueber der Knoechelmitte, soweit der eigene Knoechel es erlaubt. Die
// vordere Zehe ist um X, die hintere um Y gerollt (Krallenrollung), damit die zweite Klinge vom Kopf wegzeigt.
constexpr float kClawTip = 22.0f;
constexpr float kToeLen = 30.0f;
constexpr float kClawRollDeg = -90.0f;
constexpr float kClawBackSign = -1.0f;
constexpr float kWristRollDeg = 90.0f;   // Faust (G2): Drehung jeder Hand um den Unterarm, spiegelsymmetrisch
constexpr unsigned kAttachWindow = 30;   // Ticks der Krallenwurzel-Kalibrierung je Ritt

struct RotM {
    f32 m[3][3];
};

struct Aff {
    f32 m[3][4];  // affin, zeilenweise wie die J3D-Anim-Matrizen
};

// Pendel und Pose (posMove Post setzt, setMatrix Pre liest).
float g_swingRad = 0.0f;   // Schraeglage des Links, Radiant
float g_swingVel = 0.0f;
float g_pitchRad = 0.0f;   // Nick des Links, Radiant (Nase oben = positiv)
float g_pitchVel = 0.0f;
float g_accLong = 0.0f;    // geglaettete Vorwaertsbeschleunigung des Vogels, Einheiten je Tick^2
s16 g_ridePitch = 0;
s16 g_rideYaw = 0;
s16 g_rideRoll = 0;
bool g_pinned = false;     // Link ist in diesem Ritt auf den Griff gepinnt (pin_root)
bool g_settling = false;   // Wurzel wird durch den Handfehler korrigiert: setMatrix Pre pinnt dann nicht

// Vogelbewegung je Tick.
bool g_prevBirdValid = false;
bool g_birdVelValid = false;
cXyz g_prevBirdPos(0.0f, 0.0f, 0.0f);
cXyz g_birdVel(0.0f, 0.0f, 0.0f);

// Handmitte relativ zur Wurzel in Koerperachsen (vor Nick, Gier und Schraeglage). Gueltig nach der ersten Messung.
bool g_offValid = false;
cXyz g_offLocal(0.0f, 0.0f, 0.0f);

// Pin-Fehler (Handmitte gegen Griffmitte): Maximum, Maximum je Hand, Summe fuer den Mittelwert; Repins.
float g_errMax = 0.0f;
float g_handErrMax = 0.0f;
double g_errSum = 0.0;
uint32_t g_errN = 0;
uint32_t g_repins = 0;
uint32_t g_settleCorr = 0;

// Vogelmodell: Gelenkbaum (Vorordnung), eingefrorene Lokaltransformationen, Krallenwurzel. Je Ritt neu.
int g_parentOf[kMaxJoints];
int g_preorder[kMaxJoints];
int g_preN = 0;
bool g_frozen[kMaxJoints];
Aff g_localOf[kMaxJoints];
bool g_treeReady = false;
bool g_freezeCaptured = false;
float g_attMin[kMaxJoints][3];
float g_attMax[kMaxJoints][3];
unsigned g_attN = 0;
bool g_attachDone = false;
int g_attachParent = -1;
Aff g_attachRel;

#if defined(KFF_DEV_AUTOTEST)
constexpr uint32_t kLogEvery = 30;  // Messzeile alle 30 Ritt-Frames (Testbuild)
#endif

void grip_reset() {
    g_swingRad = 0.0f;
    g_swingVel = 0.0f;
    g_pitchRad = 0.0f;
    g_pitchVel = 0.0f;
    g_accLong = 0.0f;
    g_ridePitch = 0;
    g_rideYaw = 0;
    g_rideRoll = 0;
    g_pinned = false;
    g_settling = false;
    g_prevBirdValid = false;
    g_birdVelValid = false;
    g_prevBirdPos = cXyz(0.0f, 0.0f, 0.0f);
    g_birdVel = cXyz(0.0f, 0.0f, 0.0f);
    g_offValid = false;
    g_offLocal = cXyz(0.0f, 0.0f, 0.0f);
    g_errMax = 0.0f;
    g_handErrMax = 0.0f;
    g_errSum = 0.0;
    g_errN = 0;
    g_treeReady = false;
    g_freezeCaptured = false;
    g_attN = 0;
    g_attachDone = false;
    g_attachParent = -1;
}

// Huhn-Instanz oder nullptr, wenn sie fehlt.
fopAc_ac_c* ni_actor() {
    if (g_niId == kff::kNoProc) {
        return nullptr;
    }
    fopAc_ac_c* actor = fopAcM_SearchByID(g_niId);
    if (actor == nullptr || fopAcM_GetName(actor) != fpcNm_NI_e) {
        return nullptr;
    }
    return actor;
}

// Link haelt unsere Huhn-Instanz im nativen Greifslot.
bool holds_ours(daAlink_c* link) {
    fopAc_ac_c* ni = ni_actor();
    return ni != nullptr && link != nullptr && link->mGrabItemAcKeep.getActor() == ni;
}

// Modellrotation wie setMatrix sie baut: mDoMtx_ZXYrotM(Nick, Gier, Roll). Nur der 3x3-Teil.
RotM link_rot(s16 pitch, s16 yaw, s16 roll) {
    Mtx mtx = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};
    mDoMtx_ZXYrotM(mtx, pitch, yaw, roll);
    RotM r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = mtx[i][j];
        }
    }
    return r;
}

// R * v: Koerperachsen nach Welt (ohne Wurzelposition).
cXyz rot_apply(const RotM& r, const cXyz& v) {
    return cXyz(r.m[0][0] * v.x + r.m[0][1] * v.y + r.m[0][2] * v.z, r.m[1][0] * v.x + r.m[1][1] * v.y + r.m[1][2] * v.z,
        r.m[2][0] * v.x + r.m[2][1] * v.y + r.m[2][2] * v.z);
}

// R^T * v: Welt nach Koerperachsen (R ist eine reine Drehung, die Inverse ist die Transponierte).
cXyz rot_inverse_apply(const RotM& r, const cXyz& v) {
    return cXyz(r.m[0][0] * v.x + r.m[1][0] * v.y + r.m[2][0] * v.z, r.m[0][1] * v.x + r.m[1][1] * v.y + r.m[2][1] * v.z,
        r.m[0][2] * v.x + r.m[1][2] * v.y + r.m[2][2] * v.z);
}

float dist3(const cXyz& a, const cXyz& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Griffpunkte: je eine Faust kHandHalfSep seitlich der Mitte der beiden Knoechel (LEG_3, 0x17/0x1C) entlang der seitlichen
// Achse des Vogels, und kGripAbove darueber. Die Anim-Matrizen sind Weltkoordinaten, die Knoechel werden direkt gelesen.
bool grip_points(daKago_c* bird, cXyz& left, cXyz& right) {
    if (bird == nullptr || bird->mAnm_p == nullptr) {
        return false;
    }
    auto* model = bird->mAnm_p->getModel();
    if (model == nullptr) {
        return false;
    }
    MtxP l = model->getAnmMtx(kJntLegL3);
    MtxP r = model->getAnmMtx(kJntLegR3);
    const cXyz al(l[0][3], l[1][3], l[2][3]);
    const cXyz ar(r[0][3], r[1][3], r[2][3]);
    const cXyz mid = (al + ar) * 0.5f;
    cXyz lat(al.x - mid.x, al.y - mid.y, al.z - mid.z);
    const float lenLat = std::sqrt(lat.x * lat.x + lat.y * lat.y + lat.z * lat.z);
    if (lenLat < 1.0e-3f) {
        return false;
    }
    lat = lat * (1.0f / lenLat);
    left = mid + lat * kHandHalfSep;
    right = mid - lat * kHandHalfSep;
    left.y += kGripAbove;
    right.y += kGripAbove;
    return true;
}

#if defined(KFF_DEV_AUTOTEST)
// Tiefste Zehen- (Krallen-) Spitze des Vogels; 1e9, wenn kein Modell da ist (nur Messung).
float lowest_claw_y(daKago_c* bird) {
    if (bird == nullptr || bird->mAnm_p == nullptr || bird->mAnm_p->getModel() == nullptr) {
        return 1.0e9f;
    }
    auto* model = bird->mAnm_p->getModel();
    const int joints[4] = {kJntToeLB, kJntToeLF, kJntToeRB, kJntToeRF};
    float low = 1.0e9f;
    for (int jnt : joints) {
        MtxP m = model->getAnmMtx(jnt);
        if (m[1][3] < low) {
            low = m[1][3];
        }
    }
    return low;
}

float deg16(s16 angle) {
    return static_cast<float>(angle) * 360.0f / 65536.0f;
}
#endif

// ---------------------------------------------------------------- Gelenkmatrizen
// Affine 3x4-Matrizen, zeilenweise wie die J3D-Anim-Matrizen (Weltkoordinaten).
Aff aff_get(J3DModel* bm, int j) {
    MtxP p = bm->getAnmMtx(j);
    Aff a;
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 4; ++k) {
            a.m[i][k] = p[i][k];
        }
    }
    return a;
}

void aff_set(J3DModel* bm, int j, const Aff& a) {
    Mtx m;
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 4; ++k) {
            m[i][k] = a.m[i][k];
        }
    }
    bm->setAnmMtx(j, m);
}

Aff aff_mul(const Aff& a, const Aff& b) {
    Aff r;
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 3; ++k) {
            r.m[i][k] = a.m[i][0] * b.m[0][k] + a.m[i][1] * b.m[1][k] + a.m[i][2] * b.m[2][k];
        }
        r.m[i][3] = a.m[i][0] * b.m[0][3] + a.m[i][1] * b.m[1][3] + a.m[i][2] * b.m[2][3] + a.m[i][3];
    }
    return r;
}

// Inverse einer affinen Matrix: Kofaktoren des 3x3-Teils, Translation -R^-1 * t.
Aff aff_inv(const Aff& a) {
    const float (*x)[4] = a.m;
    const float c00 = x[1][1] * x[2][2] - x[1][2] * x[2][1];
    const float c01 = x[1][2] * x[2][0] - x[1][0] * x[2][2];
    const float c02 = x[1][0] * x[2][1] - x[1][1] * x[2][0];
    const float c10 = x[0][2] * x[2][1] - x[0][1] * x[2][2];
    const float c11 = x[0][0] * x[2][2] - x[0][2] * x[2][0];
    const float c12 = x[0][1] * x[2][0] - x[0][0] * x[2][1];
    const float c20 = x[0][1] * x[1][2] - x[0][2] * x[1][1];
    const float c21 = x[0][2] * x[1][0] - x[0][0] * x[1][2];
    const float c22 = x[0][0] * x[1][1] - x[0][1] * x[1][0];
    const float det = x[0][0] * c00 + x[0][1] * c01 + x[0][2] * c02;
    Aff r;
    if (std::fabs(det) < 1.0e-12f) {
        for (int i = 0; i < 3; ++i) {
            for (int k = 0; k < 4; ++k) {
                r.m[i][k] = (i == k) ? 1.0f : 0.0f;
            }
        }
        return r;
    }
    const float id = 1.0f / det;
    const float inv[3][3] = {{c00 * id, c10 * id, c20 * id}, {c01 * id, c11 * id, c21 * id}, {c02 * id, c12 * id, c22 * id}};
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 3; ++k) {
            r.m[i][k] = inv[i][k];
        }
        r.m[i][3] = -(inv[i][0] * x[0][3] + inv[i][1] * x[1][3] + inv[i][2] * x[2][3]);
    }
    return r;
}

cXyz aff_apply(const Aff& a, const cXyz& p) {
    return cXyz(a.m[0][0] * p.x + a.m[0][1] * p.y + a.m[0][2] * p.z + a.m[0][3],
        a.m[1][0] * p.x + a.m[1][1] * p.y + a.m[1][2] * p.z + a.m[1][3],
        a.m[2][0] * p.x + a.m[2][1] * p.y + a.m[2][2] * p.z + a.m[2][3]);
}

// Drehung um die Achse axis (Einheitsvektor nach Normierung) um den Winkel th (Rodrigues).
void axis_rot(const cXyz& axis, float th, float r[3][3]) {
    const float len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 3; ++k) {
            r[i][k] = (i == k) ? 1.0f : 0.0f;
        }
    }
    if (len < 1.0e-6f) {
        return;
    }
    const float x = axis.x / len;
    const float y = axis.y / len;
    const float z = axis.z / len;
    const float c = std::cos(th);
    const float s = std::sin(th);
    const float t = 1.0f - c;
    r[0][0] = t * x * x + c;
    r[0][1] = t * x * y - s * z;
    r[0][2] = t * x * z + s * y;
    r[1][0] = t * x * y + s * z;
    r[1][1] = t * y * y + c;
    r[1][2] = t * y * z - s * x;
    r[2][0] = t * x * z - s * y;
    r[2][1] = t * y * z + s * x;
    r[2][2] = t * z * z + c;
}

// Kleinste Drehung (um v x target, durch den Ursprung), die den Einheitsvektor v auf target bringt.
void min_turn_rot(const cXyz& v, const cXyz& target, float rot[3][3]) {
    const float dv = v.x * target.x + v.y * target.y + v.z * target.z;
    const cXyz cr(v.y * target.z - v.z * target.y, v.z * target.x - v.x * target.z, v.x * target.y - v.y * target.x);
    const float lc = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
    if (lc > 1.0e-5f) {
        axis_rot(cr, std::atan2(lc, dv), rot);
    } else if (dv > 0.0f) {
        axis_rot(cXyz(0.0f, 1.0f, 0.0f), 0.0f, rot);
    } else {  // v = -target: halbe Drehung um eine Richtung quer zu v
        cXyz across(0.0f, v.z, -v.y);  // v x X
        if (std::sqrt(across.y * across.y + across.z * across.z) < 1.0e-3f) {
            across = cXyz(-v.z, 0.0f, v.x);  // v x Y
        }
        axis_rot(across, 3.14159265f, rot);
    }
}

// Drehung eines Rahmens um seine eigene X-Achse (von rechts): Ursprung und X-Achse bleiben.
Aff turn_about_own_x(const Aff& a, float deg) {
    const float th = deg * 3.14159265f / 180.0f;
    const float c = std::cos(th);
    const float s = std::sin(th);
    Aff t = a;
    for (int i = 0; i < 3; ++i) {
        t.m[i][1] = a.m[i][1] * c + a.m[i][2] * s;
        t.m[i][2] = -a.m[i][1] * s + a.m[i][2] * c;
    }
    return t;
}

// Drehung eines Rahmens um seine eigene Y-Achse (von rechts): Ursprung und Y-Achse bleiben.
Aff turn_about_own_y(const Aff& a, float deg) {
    const float th = deg * 3.14159265f / 180.0f;
    const float c = std::cos(th);
    const float s = std::sin(th);
    Aff t = a;
    for (int i = 0; i < 3; ++i) {
        t.m[i][0] = a.m[i][0] * c - a.m[i][2] * s;
        t.m[i][2] = a.m[i][0] * s + a.m[i][2] * c;
    }
    return t;
}

// ---------------------------------------------------------------- Beinfreeze
// Gelenkbaum des Vogels (Vorordnung, Eltern vor Kindern). Eingefroren sind beide Oberschenkel und alles darunter.
void walk_tree(J3DJoint* j, int parent) {
    const int idx = static_cast<int>(j->getJntNo());
    if (idx < 0 || idx >= kMaxJoints || g_preN >= kMaxJoints) {
        return;
    }
    g_parentOf[idx] = parent;
    g_preorder[g_preN++] = idx;
    for (J3DJoint* c = j->getChild(); c != nullptr; c = c->getYounger()) {
        walk_tree(c, idx);
    }
}

void build_tree(J3DModel* bm) {
    J3DModelData* md = bm->getModelData();
    for (int i = 0; i < kMaxJoints; ++i) {
        g_parentOf[i] = -1;
        g_frozen[i] = false;
    }
    g_preN = 0;
    J3DJoint* root = md->getJointNodePointer(0);
    if (root != nullptr && static_cast<int>(md->getJointNum()) <= kMaxJoints) {
        walk_tree(root, -1);
    }
    for (int q = 0; q < g_preN; ++q) {
        const int idx = g_preorder[q];
        const int p = g_parentOf[idx];
        g_frozen[idx] = (idx == kJntLegL1 || idx == kJntLegR1) || (p >= 0 && g_frozen[p]);
    }
    g_treeReady = g_preN > 0;
}

// Die lokale Transformation jedes eingefrorenen Gelenks gegenueber seinem Elternteil wird ab Ritt-Beginn gehalten: die
// Beine bleiben dort, wo der Vogel sie hatte, und die Haende folgen nicht der Beinanimation (G2).
void capture_freeze(J3DModel* bm) {
    for (int q = 0; q < g_preN; ++q) {
        const int idx = g_preorder[q];
        const int p = g_parentOf[idx];
        if (g_frozen[idx] && p >= 0) {
            g_localOf[idx] = aff_mul(aff_inv(aff_get(bm, p)), aff_get(bm, idx));
        }
    }
    g_freezeCaptured = true;
}

void apply_freeze(J3DModel* bm) {
    for (int q = 0; q < g_preN; ++q) {
        const int idx = g_preorder[q];
        const int p = g_parentOf[idx];
        if (g_frozen[idx] && p >= 0) {
            aff_set(bm, idx, aff_mul(aff_get(bm, p), g_localOf[idx]));
        }
    }
}

// ---------------------------------------------------------------- Krallen (Zehen)
// Die Zehe ist die Klaue: die hinteren Zehen (0x18, 0x1D) zeigen in der Bindepose entlang ihres lokalen -Y, die vorderen
// (0x19, 0x1E) entlang ihres lokalen +X. Jede Zehe wird so gedreht, dass die Klaue nach vorn (vordere Zehen) bzw. nach
// hinten (hintere Zehen) zeigt, mit der Spitze kClawTip ueber der Knoechelmitte, soweit der eigene Knoechel es erlaubt.
// Danach Krallenrollung um die eigene Achse: vordere Zehen um X, hintere um Y mit Vorzeichen kClawBackSign.
void apply_toes(J3DModel* bm, s16 yaw) {
    struct Toe {
        int toe;
        float cx;  // Klauenrichtung in der Bindepose (zehenlokal)
        float cy;
        bool back;  // hintere Zehen klauen nach hinten, vordere nach vorn
    };
    const Toe specs[4] = {
        {kJntToeLB, 0.0f, -1.0f, true},
        {kJntToeLF, 1.0f, 0.0f, false},
        {kJntToeRB, 0.0f, -1.0f, true},
        {kJntToeRF, 1.0f, 0.0f, false},
    };
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    // Hoehen der beiden Knoechel und ihrer Mitte (Weltkoordinaten): ein gekippter Vogel hat einen Knoechel hoeher.
    const float yAnkleL = aff_get(bm, kJntLegL3).m[1][3];
    const float yAnkleR = aff_get(bm, kJntLegR3).m[1][3];
    const float yMid = 0.5f * (yAnkleL + yAnkleR);
    for (int q = 0; q < 4; ++q) {
        const Toe& s = specs[q];
        const Aff w = aff_get(bm, s.toe);
        const float yAnkle = (q < 2) ? yAnkleL : yAnkleR;
        const float up = std::fmin(1.0f, std::fmax(0.0f, (kClawTip - (yAnkle - yMid)) / kToeLen));
        const float ce = std::sqrt(1.0f - up * up);
        const float out = s.back ? -1.0f : 1.0f;
        const cXyz target(out * ce * hx, up, out * ce * hz);
        const cXyz v(w.m[0][0] * s.cx + w.m[0][1] * s.cy, w.m[1][0] * s.cx + w.m[1][1] * s.cy,
            w.m[2][0] * s.cx + w.m[2][1] * s.cy);
        float rot[3][3];
        min_turn_rot(v, target, rot);
        Aff n;
        for (int i = 0; i < 3; ++i) {
            for (int k = 0; k < 3; ++k) {
                n.m[i][k] = rot[i][0] * w.m[0][k] + rot[i][1] * w.m[1][k] + rot[i][2] * w.m[2][k];
            }
            n.m[i][3] = w.m[i][3];
        }
        if (s.back) {
            n = turn_about_own_y(n, -kClawRollDeg * kClawBackSign);
        } else {
            n = turn_about_own_x(n, kClawRollDeg);
        }
        aff_set(bm, s.toe, n);
    }
}

// Fuesse (r2c, G3): die lange vordere Klaue haengt an der Krallenwurzel, dem Elternteil der vorderen Zehen. Diese wird um
// ihren Ursprung so gedreht, dass ihre X-Achse nach vorn und oben zeigt (gleiche Hoehenregel wie die Zehen). Beide Zehen des
// Fusses haengen danach an der gedrehten Wurzel mit ihren eingefrorenen Lokaltransformationen.
void apply_feet(J3DModel* bm, s16 yaw) {
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    const float yAnkleL = aff_get(bm, kJntLegL3).m[1][3];
    const float yAnkleR = aff_get(bm, kJntLegR3).m[1][3];
    const float yMid = 0.5f * (yAnkleL + yAnkleR);
    const int toesB[2] = {kJntToeLB, kJntToeRB};
    const int toesF[2] = {kJntToeLF, kJntToeRF};
    for (int s = 0; s < 2; ++s) {
        const int base = g_parentOf[toesF[s]];
        if (base < 0) {
            continue;
        }
        const Aff wA = aff_get(bm, base);
        const float yA = (s == 0) ? yAnkleL : yAnkleR;
        const float up = std::fmin(1.0f, std::fmax(0.0f, (kClawTip - (yA - yMid)) / kToeLen));
        const float ce = std::sqrt(1.0f - up * up);
        const cXyz target(ce * hx, up, ce * hz);
        const cXyz axisNow(wA.m[0][0], wA.m[1][0], wA.m[2][0]);  // die X-Achse der Wurzel, Welt
        float rot[3][3];
        min_turn_rot(axisNow, target, rot);
        Aff nA;
        for (int i = 0; i < 3; ++i) {
            for (int k = 0; k < 3; ++k) {
                nA.m[i][k] = rot[i][0] * wA.m[0][k] + rot[i][1] * wA.m[1][k] + rot[i][2] * wA.m[2][k];
            }
            nA.m[i][3] = wA.m[i][3];
        }
        aff_set(bm, base, nA);
        aff_set(bm, toesB[s], aff_mul(nA, g_localOf[toesB[s]]));
        aff_set(bm, toesF[s], aff_mul(nA, g_localOf[toesF[s]]));
    }
}

// Krallenwurzel 0x23 (r3, G3): das Gelenk liegt nicht im Gelenkbaum der Zehen und haette sonst seine Klaue neben dem Kopf.
// Ueber die ersten kAttachWindow Ticks eines Rittes wird das Elternteil gesucht, in dessen Rahmen der Ursprung von 0x23
// ruht (Wert = Spannweite des Ursprungs + 0.05 x Abstand). Danach haengt 0x23 an diesem Elternteil.
void attach_calibrate(J3DModel* bm) {
    if (g_attachDone) {
        return;
    }
    const int n = static_cast<int>(bm->getModelData()->getJointNum());
    if (n <= kJntClaw || n > kMaxJoints) {
        g_attachDone = true;
#if defined(KFF_DEV_AUTOTEST)
        kff::logInfo("mensch: Krallenwurzel 0x%02x fehlt (Gelenke %d)", static_cast<unsigned>(kJntClaw), n);
#endif
        return;
    }
    const Aff c = aff_get(bm, kJntClaw);
    const cXyz oc(c.m[0][3], c.m[1][3], c.m[2][3]);
    for (int j = 0; j < n; ++j) {
        if (j == kJntClaw) {
            continue;
        }
        const cXyz loc = aff_apply(aff_inv(aff_get(bm, j)), oc);
        const float v[3] = {loc.x, loc.y, loc.z};
        for (int q = 0; q < 3; ++q) {
            if (g_attN == 0 || v[q] < g_attMin[j][q]) {
                g_attMin[j][q] = v[q];
            }
            if (g_attN == 0 || v[q] > g_attMax[j][q]) {
                g_attMax[j][q] = v[q];
            }
        }
    }
    ++g_attN;
    if (g_attN < kAttachWindow) {
        return;
    }
    int best = -1;
    float bestScore = 1.0e9f;
    float bestSpan = 0.0f;
    for (int j = 0; j < n; ++j) {
        if (j == kJntClaw) {
            continue;
        }
        float span = 0.0f;
        for (int q = 0; q < 3; ++q) {
            span += g_attMax[j][q] - g_attMin[j][q];
        }
        const Aff a = aff_get(bm, j);
        const float score = span + 0.05f * dist3(cXyz(a.m[0][3], a.m[1][3], a.m[2][3]), oc);
        if (score < bestScore) {
            bestScore = score;
            best = j;
            bestSpan = span;
        }
    }
    g_attachDone = true;
    if (best >= 0 && bestSpan < 3.0f) {
        g_attachParent = best;
        g_attachRel = aff_mul(aff_inv(aff_get(bm, best)), aff_get(bm, kJntClaw));
    }
#if defined(KFF_DEV_AUTOTEST)
    kff::logInfo("mensch: Krallenwurzel 0x%02x an Gelenk 0x%02x (Spannweite %.2f, Wert %.2f, angehaengt %d)",
        static_cast<unsigned>(kJntClaw), static_cast<unsigned>(best < 0 ? 0 : best), static_cast<double>(bestSpan),
        static_cast<double>(bestScore), g_attachParent >= 0 ? 1 : 0);
#endif
}

void attach_apply(J3DModel* bm) {
    if (g_attachParent < 0) {
        return;
    }
    aff_set(bm, kJntClaw, aff_mul(aff_get(bm, g_attachParent), g_attachRel));
}

// ---------------------------------------------------------------- Haltung, Faeuste, Pin
// Griff einhaengen: nativer Greifslot, Oberkoerper-Greifanimation, Gleit-Proc (einmal je Ritt, wie huhn2-b).
void grip_attach(daAlink_c* link, fopAc_ac_c* ni) {
    link->setGrabItemActor(ni);      // Traegerstatus und mGrabItemAcKeep (nativer Greifslot)
    link->setGrabUpperAnime(-1.0f);  // Greifanimation auf dem Oberkoerper, wie procGrabWaitInit
    link->field_0x33ec = 0.0f;       // Zweig "Huhn" von setCarryArmAngle
    link->procAutoJumpInit(1);       // nativer Gleit-Proc
}

// Griff-Waechter je Link-Frame: Greifslot, Traegerstatus, Gleit-Proc und Greifanimation halten.
void grip_keep(daAlink_c* link, fopAc_ac_c* ni) {
    if (!holds_ours(link)) {
        ++g_repairGrab;
        link->setGrabItemActor(ni);
    }
    if (!fopAcM_checkCarryNow(ni)) {
        ++g_repairCarry;
        fopAcM_setCarryNow(ni, TRUE);
    }
    if (link->mProcID != daAlink_c::PROC_AUTO_JUMP) {
        ++g_repairProc;
        link->procAutoJumpInit(0);  // Reparatur wie huhn2-b (Watchdog)
    }
    if (!link->checkGrabAnime()) {
        ++g_repairUpper;
        link->setGrabUpperAnime(-1.0f);
    }
}

// Faeuste: Handformen 0 (links) und 6 (rechts), keine Ueberschreibung durch die native Handform (symmetrisch).
void grip_fist(daAlink_c* link) {
    link->mLeftHandIndex = kFistLeft;
    link->mRightHandIndex = kFistRight;
    link->field_0x2f94 = 0xFF;
    link->field_0x2f95 = 0xFF;
    link->field_0x2f96 = 0xFF;
    link->field_0x2f97 = 0xFF;
}

// Arme gestreckt (setArmMatrix Pre): Ellbogenwinkel 0 an beiden Armen, also symmetrisch.
void grip_arms(daAlink_c* link) {
    link->field_0x3136[0].y = 0;
    link->field_0x3136[1].y = 0;
}

// Pose: Kurs des Vogels, Schraeglage und Nick aus den Pendeln (setMatrix Pre und posMove Post).
void grip_pose(daAlink_c* link) {
    link->shape_angle.x = g_ridePitch;
    link->shape_angle.y = g_rideYaw;
    link->shape_angle.z = g_rideRoll;
    link->current.angle.x = g_ridePitch;
    link->current.angle.y = g_rideYaw;
    link->current.angle.z = g_rideRoll;
    link->field_0x308c = 0;
}

// Pin: die Handmitte von Link landet auf der Griffmitte. Der Handabstand von der Wurzel ist der zuletzt gemessene (mit der
// Pose gedreht); vor der ersten Messung gilt die Standardhoehe.
void pin_root(daAlink_c* link, const cXyz& grip) {
    const cXyz off = g_offValid ? rot_apply(link_rot(g_ridePitch, g_rideYaw, g_rideRoll), g_offLocal)
                                : cXyz(0.0f, kHandsUpDefault, 0.0f);
    const cXyz target = grip - off;
    link->current.pos = target;
    link->old.pos = target;
    link->speed = cXyz::Zero;
    link->mFallHeight = target.y;
    link->field_0x33ec = 0.0f;
    g_pinned = true;
}

// Modell von Link aus Wurzel und Pose neu aufbauen. Die Wurzel-Korrektur (repin) darf dabei nicht durch den Pin in
// setMatrix Pre ueberschrieben werden: g_settling schaltet diesen Pin ab.
void build_link_pose(daAlink_c* link) {
    g_settling = true;
    link->setMatrix();
    g_settling = false;
    if (link->mpLinkModel != nullptr) {
        link->mpLinkModel->calc();
    }
    link->setBodyPartPos();
}

// Fausten (G2, r4): jede Hand dreht sich um ihren Unterarm (Ellbogen zum Handgelenk), die rechte in die Gegenrichtung.
// Nach dem Armaufbau dieses Ticks aufrufen (repin), damit die Drehung nicht doppelt wirkt.
void apply_wrist_roll(daAlink_c* link) {
    if (link == nullptr || link->mpLinkModel == nullptr) {
        return;
    }
    J3DModel* lm = link->mpLinkModel;
    const int pairs[2][2] = {{kLinkJntElbowL, kLinkJntHandL}, {kLinkJntElbowR, kLinkJntHandR}};
    const float signs[2] = {1.0f, -1.0f};
    for (int s = 0; s < 2; ++s) {
        const Aff e = aff_get(lm, pairs[s][0]);
        const Aff h = aff_get(lm, pairs[s][1]);
        cXyz ax(h.m[0][3] - e.m[0][3], h.m[1][3] - e.m[1][3], h.m[2][3] - e.m[2][3]);
        const float len = std::sqrt(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z);
        if (len < 1.0e-3f) {
            continue;
        }
        ax = ax * (1.0f / len);
        float rot[3][3];
        axis_rot(ax, signs[s] * kWristRollDeg * 3.14159265f / 180.0f, rot);
        Aff nh = h;  // der Handgelenkursprung (Spalte 3) bleibt, die Drehung wirkt auf den Rahmen der Hand
        for (int i = 0; i < 3; ++i) {
            for (int k = 0; k < 3; ++k) {
                nh.m[i][k] = rot[i][0] * h.m[0][k] + rot[i][1] * h.m[1][k] + rot[i][2] * h.m[2][k];
            }
        }
        aff_set(lm, pairs[s][1], nh);
    }
}

// Vogel-Seite des Repins (G1, r3/r5): nach dem Vogel-Tick wird Link erneut auf den Griff dieses Ticks gepinnt. Die Wurzel
// wird um den gemessenen Handfehler korrigiert: am ersten Tick ohne Korrektur, dann bis zu acht Durchgaenge (ohne gemessenen
// Handabstand) oder drei.
void repin_after_bird(daAlink_c* link, daKago_c* bird) {
    cXyz gl;
    cXyz gr;
    if (!grip_points(bird, gl, gr)) {
        return;
    }
    const cXyz grip = (gl + gr) * 0.5f;
    pin_root(link, grip);
    const uint32_t f = kff::session().rideFrames;
    const int passes = (f == 1) ? 0 : ((g_offValid && f > 3) ? 3 : 8);
    bool settled = false;
    for (int pass = 0; pass < passes; ++pass) {
        build_link_pose(link);
        const cXyz mid = (link->mLeftHandPos + link->mRightHandPos) * 0.5f;
        if (dist3(grip, mid) < 0.3f) {
            settled = true;
            break;
        }
        link->current.pos = link->current.pos + (grip - mid);
        link->old.pos = link->current.pos;
        ++g_settleCorr;
    }
    if (!settled) {
        build_link_pose(link);
    }
    grip_fist(link);
    apply_wrist_roll(link);
    ++g_repins;
}

// Vogel-Bewegung je Vogel-Tick: Vorwaertsbeschleunigung fuer den Nick; das Modell des Vogels folgt dem Kurs.
void update_bird_motion(daKago_c* k) {
    const cXyz p = k->current.pos;
    if (g_prevBirdValid) {
        const cXyz v(p.x - g_prevBirdPos.x, p.y - g_prevBirdPos.y, p.z - g_prevBirdPos.z);
        if (g_birdVelValid) {
            const cXyz a(v.x - g_birdVel.x, v.y - g_birdVel.y, v.z - g_birdVel.z);
            const s16 h = k->current.angle.y;
            const float along = a.x * cM_ssin(h) + a.z * cM_scos(h);
            g_accLong = g_accLong * (1.0f - kAccSmooth) + along * kAccSmooth;
        }
        g_birdVel = v;
        g_birdVelValid = true;
    }
    g_prevBirdPos = p;
    g_prevBirdValid = true;
    k->shape_angle.y = k->current.angle.y;
}

// Messung (setBodyPartPos Post): Handmitte relativ zur Wurzel fuer den naechsten Pin, Pin-Fehler (Statistik fuer das
// Ritt-Ende). Die Spur- und Messzeilen gibt es nur im Testbuild.
void grip_measure(daAlink_c* link, daKago_c* bird) {
    const cXyz hands = (link->mLeftHandPos + link->mRightHandPos) * 0.5f;
    g_offLocal = rot_inverse_apply(link_rot(link->shape_angle.x, link->shape_angle.y, link->shape_angle.z),
        hands - link->current.pos);
    g_offValid = true;

    cXyz gl;
    cXyz gr;
    if (!grip_points(bird, gl, gr)) {
        return;
    }
    const cXyz grip = (gl + gr) * 0.5f;
    const float err = dist3(hands, grip);
    const float errL = dist3(link->mLeftHandPos, gl);
    const float errR = dist3(link->mRightHandPos, gr);
    const float handErr = errL > errR ? errL : errR;
    if (err > g_errMax) {
        g_errMax = err;
    }
    if (handErr > g_handErrMax) {
        g_handErrMax = handErr;
    }
    g_errSum += static_cast<double>(err);
    ++g_errN;

#if defined(KFF_DEV_AUTOTEST)
    const uint32_t f = kff::session().rideFrames;
    const float ankleSep = dist3(gl, gr);
    const float handSep = dist3(link->mLeftHandPos, link->mRightHandPos);
    kff::logInfo("mensch: spur f=%u err=%.2f errL=%.2f errR=%.2f ankleSep=%.2f handSep=%.2f rollBird=%.2f rollLink=%.2f "
        "pitchBird=%.2f proc=%u holds=%d",
        f, static_cast<double>(err), static_cast<double>(errL), static_cast<double>(errR), static_cast<double>(ankleSep),
        static_cast<double>(handSep), static_cast<double>(deg16(bird->shape_angle.z)),
        static_cast<double>(deg16(link->shape_angle.z)), static_cast<double>(deg16(bird->shape_angle.x)),
        static_cast<unsigned>(link->mProcID), holds_ours(link) ? 1 : 0);
    if (f % kLogEvery == 0) {
        const float clawGap = lowest_claw_y(bird) - link->mHeadTopPos.y;  // > 0: Kopf liegt unter den Krallen
        const float groundH = link->mLinkAcch.GetGroundH();
        const bool groundOk = groundH > -1.0e8f && groundH < 1.0e8f;  // ohne Boden meldet die Engine etwa 1e9
        const float feet = groundOk ? link->current.pos.y - groundH : -1.0f;
        const float yawDiff = deg16(static_cast<s16>(bird->shape_angle.y - link->shape_angle.y));
        kff::logInfo("mensch: messung f=%u proc=%u holds=%d err=%.1f errL=%.1f errR=%.1f clawGap=%.1f feet=%.1f "
            "groundOk=%d rollPose=%.1f yawDiffDeg=%.1f hands=(%.1f,%.1f,%.1f) grip=(%.1f,%.1f,%.1f) "
            "birdPos=(%.1f,%.1f,%.1f) linkPos=(%.1f,%.1f,%.1f) blocked=%u",
            f, static_cast<unsigned>(link->mProcID), holds_ours(link) ? 1 : 0, static_cast<double>(err),
            static_cast<double>(errL), static_cast<double>(errR), static_cast<double>(clawGap),
            static_cast<double>(feet), groundOk ? 1 : 0, static_cast<double>(deg16(g_rideRoll)),
            static_cast<double>(yawDiff), static_cast<double>(hands.x), static_cast<double>(hands.y),
            static_cast<double>(hands.z), static_cast<double>(grip.x), static_cast<double>(grip.y),
            static_cast<double>(grip.z), static_cast<double>(bird->current.pos.x), static_cast<double>(bird->current.pos.y),
            static_cast<double>(bird->current.pos.z), static_cast<double>(link->current.pos.x),
            static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z), g_releaseBlocked);
    }
#endif
}

// ===== GRIFF-ABSCHNITT END ==================================================================================

// ---------------------------------------------------------------------------------------------
// Lebenszyklus des Griffs: Ritt-Start, Griff-Waechter, Ritt-Ende.
// ---------------------------------------------------------------------------------------------

// Ritt-Start (Kern, Vogel bereit): Huhn-Instanz im Link-Ort anlegen, Griff ausstehend.
void begin_grip(daAlink_c* link) {
    if (fopAc_ac_c* old = ni_actor()) {
        fopAcM_delete(old);  // Rest eines frueheren Griffs
    }
    g_niId = kff::kNoProc;
    const int room = static_cast<int>(fopAcM_GetRoomNo(link));
    const cXyz pos(link->current.pos.x, link->current.pos.y, link->current.pos.z);
    const csXyz angle(0, link->shape_angle.y, 0);
    const fpc_ProcID id = fopAcM_create(fpcNm_NI_e, 0u, &pos, room, &angle, nullptr, -1);
    if (id == fpcM_ERROR_PROCESS_ID_e) {
        kff::logError("mensch: Huhn-Instanz nicht angelegt (fopAcM_create), Griff faellt aus");
    } else {
        g_niId = id;
    }
    g_grip = Grip::Pending;
    g_pendingFrames = 0;
    grip_reset();
    kff::logInfo("mensch: Ritt-Start im Griff, Huhn-Instanz 0x%x, Link-Aufrufe %u", static_cast<unsigned>(g_niId),
        kff::session().linkCalls);
}

// Ritt-Ende (Kern ruft es vor dem Loesen von Link): Griff freigeben, Huhn-Instanz entfernen, Zustand zuruecksetzen.
void end_grip(daAlink_c* link, kff::EndReason why) {
    const bool wasHolding = g_grip == Grip::Holding;
    if (link != nullptr && holds_ours(link)) {
        g_releaseAllowed = true;  // das eigene Loslassen ist erlaubt, nur hier
        link->freeGrabItem();
        g_releaseAllowed = false;
    }
    if (fopAc_ac_c* ni = ni_actor()) {
        fopAcM_delete(ni);
    }
    kff::logInfo("mensch: Griff beendet (%s), gehalten=%d, Pin-Fehler max=%.1f Hand max=%.1f mittel=%.1f (n=%u), "
        "Loslassen blockiert=%u, Waechter grab=%u carry=%u proc=%u upper=%u, Repins=%u",
        kff::endReasonName(why), wasHolding ? 1 : 0, static_cast<double>(g_errMax), static_cast<double>(g_handErrMax),
        static_cast<double>(g_errN > 0 ? g_errSum / static_cast<double>(g_errN) : 0.0), g_errN, g_releaseBlocked,
        g_repairGrab, g_repairCarry, g_repairProc, g_repairUpper, g_repins);
    g_niId = kff::kNoProc;
    g_grip = Grip::Off;
    g_pendingFrames = 0;
    grip_reset();
    ++g_ends;
}

// Ruf als Mensch (onHumanCall): mit mensch_ritt AUS false (autowolf verwandelt), sonst Vogel rufen und Griff vormerken.
bool human_call(kff::Session& s, daAlink_c* link) {
    if (!kff::config::getBool(kff::config::Key::MenschRitt)) {
        // WP5.14: der Mensch-Ritt (OE3) ist aus. Der Ruf als Mensch wird hier abgelehnt; autowolf entscheidet danach
        // ueber die Verwandlung (sonst meldet der Kern "human form - ride needs wolf").
        static uint32_t logHumanOff = 0;
        kff::logEvery(logHumanOff, 5, 60,
            "mensch: call rejected (human): Mensch-Ritt aus (mensch_ritt=false, D4), der Ruf geht an autowolf");
        return false;  // Entscheidung D4: Standard AUS, dann verwandelt autowolf
    }
    if (link == nullptr) {
        return true;
    }
    // N1: ein abfliegender eigener Vogel blockiert den Ruf nicht (summonBird entfernt ihn beim Annehmen).
    if (s.phase != kff::Phase::Idle && !kff::birdDeparting(s)) {
        kff::logInfo("mensch: Ruf als Mensch abgelehnt, Phase %s", kff::phaseName(s.phase));
        return true;  // Ritt oder Ruf laeuft: der Ruf ist verbraucht, autowolf darf nicht verwandeln
    }
    if (link->checkEventRun() || dComIfGp_event_runCheck()) {
        kff::logInfo("mensch: Ruf als Mensch abgelehnt (Ereignis)");
        return true;
    }
    if (kff::summonBird(link) != MOD_OK) {
        kff::logError("mensch: Vogel nicht gerufen (summonBird)");
        return true;
    }
    // summonBird setzt die Sitzung neu (resetSession), danach gilt der Ritt im Griff fuer dieses Rufen.
    s.rideGrip = true;
    ++g_calls;
    kff::logInfo("mensch: Ruf als Mensch angenommen, Vogel gerufen, Ritt im Griff (Aufruf %u)", g_calls);
    return true;
}

// Griff-Waechter (onFrameLink, vor daAlink_c::execute): Start des Griffs, Halt, geordnetes Ende.
void watch_grip(kff::Session& s, daAlink_c* link) {
    if (!s.rideGrip || s.phase != kff::Phase::Riding || link == nullptr || g_grip == Grip::Off) {
        return;
    }
    daKago_c* bird = kff::liveBird();
    if (bird == nullptr) {
        return;  // der Kern meldet den Verlust des Vogels
    }
    if (link->mProcID == daAlink_c::PROC_DEAD) {
        return;  // Tod: der Kern beendet den Ritt (endRideClean)
    }
    if (link->checkWolf() != 0) {
        kff::logInfo("mensch: Link ist Wolf im Ritt, Ritt endet geordnet");
        kff::dismount(bird, kff::EndReason::RiderLost, "Link ist Wolf");
        return;
    }
    if (dComIfGp_event_runCheck()) {
        kff::dismount(bird, kff::EndReason::Event, "Ereignis im Ritt");
        return;
    }

    fopAc_ac_c* ni = ni_actor();
    if (g_grip == Grip::Pending) {
        ++g_pendingFrames;
        if (ni != nullptr && g_pendingFrames >= kAttachAfterFrames) {
            grip_attach(link, ni);
            g_grip = Grip::Holding;
            ++g_starts;
            kff::logInfo("mensch: Griff eingehaengt nach %u Link-Frames (Ritt-Frame %u)", g_pendingFrames, s.rideFrames);
        } else if (g_pendingFrames > kPendingMaxFrames) {
            kff::logError("mensch: Griff kam nicht zustande (Huhn-Instanz %d), Ritt endet geordnet", ni != nullptr ? 1 : 0);
            kff::dismount(bird, kff::EndReason::RiderLost, "Griff nicht gestartet");
        }
        return;
    }
    // Holding: Haltung sichern, Handform setzen.
    if (ni == nullptr) {
        kff::logError("mensch: Huhn-Instanz fehlt im Ritt, Ritt endet geordnet");
        kff::dismount(bird, kff::EndReason::RiderLost, "Huhn-Instanz fehlt");
        return;
    }
    grip_keep(link, ni);
    grip_fist(link);
}

// ---------------------------------------------------------------------------------------------
// Hook-Callbacks (Kern-Registry). Jeder Callback faengt Ausnahmen selbst ab, damit ein Fehler nur diesen Frame kostet.
// ---------------------------------------------------------------------------------------------

// daAlink_c::execute, Post: Huhn-Instanz unsichtbar halten, Handform fuer das Zeichnen.
void on_link_execute_post(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr || g_grip == Grip::Off) {
            return;
        }
        if (fopAc_ac_c* ni = ni_actor()) {
            fopAcM_OnStatus(ni, fopAcStts_NODRAW_e);
        }
        if (g_grip == Grip::Holding && holds_ours(link)) {
            grip_fist(link);
        }
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in daAlink_c::execute (Post), Frame uebersprungen");
    }
}

// daKago_c::execute, Post (nur der eigene Vogel): Beine, Fuesse, Krallen, Krallenwurzel und Repin von Link.
void on_kago_execute_post(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        if (k == nullptr || g_grip == Grip::Off || k->mAnm_p == nullptr || k != kff::liveBird()) {
            return;
        }
        J3DModel* bm = k->mAnm_p->getModel();
        if (bm == nullptr) {
            return;
        }
        if (g_grip == Grip::Pending) {
            // Vor dem Griff: Beine einfrieren, damit die Haltung beim Einhaengen dem Vogel entspricht (r5, G1).
            if (!g_treeReady) {
                build_tree(bm);
            }
            if (g_treeReady) {
                if (!g_freezeCaptured) {
                    capture_freeze(bm);
                }
                apply_freeze(bm);
                bm->calcWeightEnvelopeMtx();
            }
            return;
        }
        if (g_grip != Grip::Holding) {
            return;
        }
        update_bird_motion(k);
        if (!g_treeReady) {
            build_tree(bm);
        }
        attach_calibrate(bm);  // Matrizen dieses Ticks, vor jeder Aenderung
        if (g_treeReady) {
            if (!g_freezeCaptured) {
                capture_freeze(bm);
            }
            apply_freeze(bm);
        }
        if (g_freezeCaptured) {
            apply_feet(bm, k->current.angle.y);
        }
        apply_toes(bm, k->current.angle.y);
        attach_apply(bm);
        bm->calcWeightEnvelopeMtx();
        daAlink_c* link = kff::liveLink();
        if (link != nullptr && g_pinned && holds_ours(link)) {
            repin_after_bird(link, k);
        }
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in daKago_c::execute (Post), Vogel-Seite uebersprungen");
    }
}

// daAlink_c::posMove, Post: Pendel fuer Schraeglage und Nick, Pose, Pin.
void on_link_pos_move_post(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr || g_grip != Grip::Holding || !holds_ours(link)) {
            return;
        }
        if (kff::session().phase != kff::Phase::Riding) {
            return;
        }
        daKago_c* bird = kff::liveBird();
        if (bird == nullptr) {
            return;
        }
        cXyz gl;
        cXyz gr;
        if (!grip_points(bird, gl, gr)) {
            return;
        }
        const cXyz grip = (gl + gr) * 0.5f;

        // Schraeglage: gedaempftes Pendel auf die halbe Schraeglage des Vogels (r5, G6). Keine harte Begrenzung.
        const float bankTarget = static_cast<float>(bird->shape_angle.z) * kRad16 * kBankScale;
        const float accR = kSwingOmega * kSwingOmega * (bankTarget - g_swingRad) - 2.0f * kSwingZeta * kSwingOmega * g_swingVel;
        g_swingVel += accR * kSimDt;
        g_swingRad += g_swingVel * kSimDt;
        // Nick: gedaempftes Pendel auf die Vorwaertsbeschleunigung; die Sollage saettigt bei kPitchMax (tanh).
        const float pitchTarget = kPitchMax * std::tanh(-kPitchGain * g_accLong / kPitchMax);
        const float accP = kSwingOmega * kSwingOmega * (pitchTarget - g_pitchRad) - 2.0f * kPitchZeta * kSwingOmega * g_pitchVel;
        g_pitchVel += accP * kSimDt;
        g_pitchRad += g_pitchVel * kSimDt;
        g_ridePitch = static_cast<s16>(g_pitchRad / kRad16);
        g_rideRoll = static_cast<s16>(g_swingRad / kRad16);
        g_rideYaw = bird->shape_angle.y;

        grip_pose(link);
        pin_root(link, grip);
        link->setCarryArmAngle(0.0f, 1.0f);  // Huhn-Ellbogenwinkel; setArmMatrix skaliert sie (0 = gestreckt)
        grip_fist(link);
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in posMove (Post), Frame uebersprungen");
    }
}

// daAlink_c::setMatrix, Pre: Pose aus den Pendeln; der Pin wird vor dem Aufbau der Matrix wiederholt (die native
// Bodenkorrektur verschiebt wie nach posMove die Wurzel), ausser bei der Wurzel-Korrektur (g_settling).
HookAction on_link_set_matrix_pre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link != nullptr && g_grip == Grip::Holding && holds_ours(link)) {
            grip_pose(link);
            daKago_c* bird = kff::liveBird();
            cXyz gl;
            cXyz gr;
            if (!g_settling && bird != nullptr && grip_points(bird, gl, gr)) {
                pin_root(link, (gl + gr) * 0.5f);
            }
        }
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in setMatrix (Pre), Frame uebersprungen");
    }
    return HOOK_CONTINUE;
}

// daAlink_c::setBodyPartPos, Post: Messung der Handmitte.
void on_link_body_part_post(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr || g_grip != Grip::Holding || !holds_ours(link)) {
            return;
        }
        daKago_c* bird = kff::liveBird();
        if (bird == nullptr) {
            return;
        }
        grip_measure(link, bird);
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in setBodyPartPos (Post), Messung uebersprungen");
    }
}

// daAlink_c::setArmMatrix, Pre: gestreckte Arme.
HookAction on_link_arm_pre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link != nullptr && g_grip == Grip::Holding && holds_ours(link)) {
            grip_arms(link);
        }
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in setArmMatrix (Pre), Frame uebersprungen");
    }
    return HOOK_CONTINUE;
}

// daAlink_c::freeGrabItem, Pre: im Griff wird nicht losgelassen, ausser beim eigenen Ritt-Ende (nur B loest).
HookAction on_link_free_grab_pre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (!g_releaseAllowed && g_grip == Grip::Holding && holds_ours(link)) {
            ++g_releaseBlocked;
            kff::logEvery(g_logBlock, 3, 60, "mensch: Loslassen im Griff blockiert (nur B loest), Anzahl %u",
                g_releaseBlocked);
            return HOOK_SKIP_ORIGINAL;
        }
    } catch (...) {
        kff::logEvery(g_logExc, 3, 300, "mensch: Ausnahme in freeGrabItem (Pre), Loslassen erlaubt");
    }
    return HOOK_CONTINUE;
}

// ---------------------------------------------------------------------------------------------
// Modul
// ---------------------------------------------------------------------------------------------

#if defined(KFF_DEV_AUTOTEST)
// Szenario "mensch" (Testbuild): Inventar und Herzen vor dem Ruf, Ruf als Mensch (R halten, B tippen), Ritt im Griff mit
// Hoehenhalt, ein voller Linkskreis und ein voller Rechtskreis nahe am Startpunkt, B zum Absteigen ueber dem Boden, Landung,
// dann 20 s laufen. Eingaben nur ueber den Autotest-Kontext. Die Messung laeuft im Modul (Zeilen spur und messung).
//
// Hoehen- und Kurssteuerung (nur Test): Stick Y > 0 = Nase nach unten (flug.cpp). Ein Nickregler stellt den Nick des
// Vogels auf den Sollwert: -10 Grad (Nase hoch) solange der Vogel unter Start-Hoehe + kMeHoldAbove liegt, sonst 0 Grad
// (waagerecht). Stick X = -1 (Linkskreis) bzw. +1 (Rechtskreis): 128 Frames voll = 360 Grad (flug.cpp), also ist jeder
// Kreis geschlossen und der Vogel bleibt in der Naehe des Startpunkts.
constexpr uint32_t kMeCallAt = 240;        // erster Ruf, sobald Link bereit ist (Ticks ab dem ersten Link-Frame)
constexpr uint32_t kMeHold = 30;           // R gehalten, Ticks
constexpr uint32_t kMePressOffset = 15;    // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kMeClimbFirst = 60;     // erste Ritt-Frames immer steigen (Sollnick -10 Grad)
constexpr float kMeHoldAbove = 300.0f;     // Zielhoehe = Ritt-Start-Hoehe + diese Einheiten
constexpr float kMeClimbNick = -10.0f;     // Sollnick beim Steigen in Grad (negativ = Nase hoch)
constexpr float kMeNickGain = 0.02f;       // Stick Y je Grad Nickabweichung (Regler)
constexpr uint32_t kMeLeftFrom = 60;       // Linkskreis: Frames 60 bis 188 (128 Frames = 360 Grad)
constexpr uint32_t kMeLeftTo = 188;
constexpr uint32_t kMeRightFrom = 248;     // Rechtskreis: Frames 248 bis 376
constexpr uint32_t kMeRightTo = 376;
constexpr uint32_t kMeDismountAt = 436;    // B zum Absteigen bei Ritt-Frame 436 (Vogel nahe Startpunkt)
constexpr uint32_t kMeLandMinAfter = 10;   // Landung frueheststens so viele Ticks nach dem Absteigen pruefen
constexpr uint32_t kMeLandLimit = 900;     // ohne Landung nach so vielen Ticks: FAIL und Ende
constexpr uint32_t kMeWalkTicks = 600;     // 20 s laufen (30 Ticks je Sekunde)
constexpr uint32_t kMeLogEvery = 60;       // Link-Zustand alle 60 Ticks nach dem Absteigen

struct MeState {
    bool armed = false;
    uint32_t callStart = 0;
    bool rideSeen = false;
    float baseY = 0.0f;
    bool dismounted = false;
    uint32_t dismountedAt = 0;
    bool landed = false;
    uint32_t landedAt = 0;
    float landX = 0.0f;
    float landZ = 0.0f;
    bool done = false;
};
MeState g_me;

// Ruf als Mensch moeglich: Link ist Mensch, sitzt nicht, kein Ereignis, kein eigener Vogel, keine Sitzung.
bool me_can_call(const kff::autotest::Context& c) {
    daAlink_c* link = kff::liveLink();
    return link != nullptr && link->checkWolf() == 0 && !link->checkCargoCarry() && !link->checkEventRun() &&
           !dComIfGp_event_runCheck() && c.sess.phase == kff::Phase::Idle && c.sess.birdId == kff::kNoProc;
}

// Link-Zustand als Log-Zeile des Szenarios (Position, Proc, Boden, Abstand zum Boden).
void me_note_link(kff::autotest::Context& c, const char* what) {
    char buf[260];
    daAlink_c* link = kff::liveLink();
    if (link == nullptr) {
        std::snprintf(buf, sizeof(buf), "%s: kein Link", what);
    } else {
        const float gh = link->mLinkAcch.GetGroundH();
        std::snprintf(buf, sizeof(buf), "%s: link=(%.1f,%.1f,%.1f) proc=%u boden=%d groundH=%.1f feet=%.1f", what,
            static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
            static_cast<double>(link->current.pos.z), static_cast<unsigned>(link->mProcID),
            link->mLinkAcch.ChkGroundHit() ? 1 : 0, static_cast<double>(gh),
            static_cast<double>(link->current.pos.y - gh));
    }
    c.note(buf);
}

// Herzen (Lebensenergie) als Log-Zeile: vor dem Ritt, vor dem Absteigen, nach der Landung und am Ende.
void me_note_hearts(kff::autotest::Context& c, const char* what) {
    char buf[120];
    std::snprintf(buf, sizeof(buf), "%s: leben=%u maxLeben=%u", what, static_cast<unsigned>(dComIfGs_getLife()),
        static_cast<unsigned>(dComIfGs_getMaxLife()));
    c.note(buf);
}

// Inventar als Log-Zeile: Hakenschuss und Schattenkristall aus dem Spielstand (Startzustand: beide 0).
void me_note_inventory(kff::autotest::Context& c, const char* what) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s: hookshot=%u shadowCrystal=%u", what,
        static_cast<unsigned>(dComIfGs_checkGetItem(static_cast<u8>(dItemNo_HOOKSHOT_e))),
        static_cast<unsigned>(dComIfGs_checkGetItem(static_cast<u8>(dItemNo_SHADOW_CRYSTAL_e))));
    c.note(buf);
}

// Hoehen- und Kurssteuerung des Tests: Nickregler auf den Sollwert, Stick X fuer die Kreise.
void me_stick(uint32_t f, const daKago_c* bird, float baseY, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    if (bird == nullptr) {
        return;
    }
    const bool climb = f < kMeClimbFirst || bird->current.pos.y < baseY + kMeHoldAbove;
    const float target = climb ? kMeClimbNick : 0.0f;
    const float nick = static_cast<float>(bird->shape_angle.x) * 360.0f / 65536.0f;
    float yy = -kMeNickGain * (nick - target);
    if (yy > 0.6f) {
        yy = 0.6f;
    }
    if (yy < -0.6f) {
        yy = -0.6f;
    }
    y = yy;
    if (f >= kMeLeftFrom && f < kMeLeftTo) {
        x = -1.0f;
    } else if (f >= kMeRightFrom && f < kMeRightTo) {
        x = 1.0f;
    }
}

bool me_tick(kff::autotest::Context& c) {
    const uint32_t t = c.frame;
    if (!g_me.armed && t >= kMeCallAt && me_can_call(c)) {
        g_me.armed = true;
        g_me.callStart = t;
        me_note_inventory(c, "Inventar vor dem Ruf");
        me_note_hearts(c, "Herzen vor dem Ruf");
        c.note("Ruf als Mensch: R halten, B tippen");
    }
    if (g_me.armed && !g_me.rideSeen) {
        if (t >= g_me.callStart && t < g_me.callStart + kMeHold) {
            c.holdR();
        }
        if (t == g_me.callStart + kMePressOffset) {
            c.tapB();
            c.note("B im R-Halt (Ruf als Mensch)");
        }
    }

    if (c.sess.phase == kff::Phase::Riding) {
        daKago_c* bird = kff::liveBird();
        if (!g_me.rideSeen) {
            g_me.rideSeen = true;
            g_me.baseY = bird != nullptr ? bird->current.pos.y : 0.0f;
            daAlink_c* link = kff::liveLink();
            if (link != nullptr && link->checkWolf() == 0) {
                c.note("Ritt beginnt als Mensch");
            } else {
                c.note("Ritt beginnt, Link ist kein Mensch (FAIL: Mensch-Ritt nicht aktiv, mensch_ritt pruefen)");
            }
            me_note_hearts(c, "Herzen vor dem Ritt");
        }
        const uint32_t f = c.sess.rideFrames;
        // Foto-Marken (Testlaeufe mit run-wp5.sh: "wp5shot: <name>" loest ein Foto des Spielfensters aus).
        if (f == 30 || f == 120 || f == 310 || f == 430) {
            kff::logInfo("wp5shot: mensch-f%u", f);
        }
        if (!g_me.dismounted && f == kMeDismountAt) {
            g_me.dismounted = true;
            g_me.dismountedAt = t;
            me_note_hearts(c, "Herzen vor dem Absteigen");
            c.tapB();
            c.note("B-Tipp: Absteigen (Vogel nahe Startpunkt, ueber dem Boden)");
        }
        if (!g_me.dismounted) {
            float x = 0.0f;
            float y = 0.0f;
            me_stick(f, bird, g_me.baseY, x, y);
            c.stick(x, y);
            if (f == kMeLeftFrom) {
                c.note("Linkskreis beginnt (Stick X=-1, 128 Frames = 360 Grad)");
            }
            if (f == kMeLeftTo) {
                c.note("Linkskreis Ende");
            }
            if (f == kMeRightFrom) {
                c.note("Rechtskreis beginnt (Stick X=+1, 128 Frames = 360 Grad)");
            }
            if (f == kMeRightTo) {
                c.note("Rechtskreis Ende");
            }
            if (f % 120u == 0u && bird != nullptr) {
                char buf[200];
                std::snprintf(buf, sizeof(buf), "Ritt f=%u vogelY=%.1f baseY=%.1f nick=%.1f stick=(%.2f,%.2f)", f,
                    static_cast<double>(bird->current.pos.y), static_cast<double>(g_me.baseY),
                    static_cast<double>(static_cast<float>(bird->shape_angle.x) * 360.0f / 65536.0f),
                    static_cast<double>(x), static_cast<double>(y));
                c.note(buf);
            }
        }
    }
    // Ritt ohne B-Tipp beendet (Kern): gilt wie Absteigen fuer die Landungspruefung.
    if (g_me.rideSeen && !g_me.dismounted && c.sess.phase != kff::Phase::Riding) {
        g_me.dismounted = true;
        g_me.dismountedAt = t;
        c.note("Ritt ohne B-Tipp beendet (Kern)");
    }

    if (g_me.dismounted && !g_me.landed && !g_me.done) {
        if ((t - g_me.dismountedAt) % kMeLogEvery == 0u) {
            me_note_link(c, "nach Absteigen");
        }
        if (t - g_me.dismountedAt > kMeLandLimit) {
            g_me.done = true;
            me_note_link(c, "FAIL ohne Landung");
            c.note("FAIL: keine Landung innerhalb von 900 Ticks nach dem Absteigen");
            return true;
        }
        daAlink_c* link = kff::liveLink();
        const bool onFoot = link != nullptr && t - g_me.dismountedAt >= kMeLandMinAfter &&
                            c.sess.phase != kff::Phase::Riding && !dComIfGp_event_runCheck() && !link->checkEventRun() &&
                            link->mProcID != daAlink_c::PROC_AUTO_JUMP && link->mProcID != daAlink_c::PROC_FALL &&
                            link->mLinkAcch.ChkGroundHit();
        if (onFoot) {
            g_me.landed = true;
            g_me.landedAt = t;
            g_me.landX = link->current.pos.x;
            g_me.landZ = link->current.pos.z;
            me_note_link(c, "Landung");
            c.note("Landung auf dem Boden");
            kff::logInfo("wp5shot: mensch-landung");
            me_note_hearts(c, "Herzen nach Landung");
        }
    }
    if (g_me.landed && !g_me.done) {
        if (t - g_me.landedAt < kMeWalkTicks) {
            c.stick(0.0f, 0.8f);  // Stick hoch = vorwaerts zu Fuss
            if ((t - g_me.landedAt) % 120u == 0u) {
                me_note_link(c, "Gehen");
            }
        } else {
            g_me.done = true;
            daAlink_c* link = kff::liveLink();
            if (link != nullptr) {
                const float dx = link->current.pos.x - g_me.landX;
                const float dz = link->current.pos.z - g_me.landZ;
                char buf[200];
                std::snprintf(buf, sizeof(buf), "Gehen: Verschiebung dx=%.1f dz=%.1f Strecke=%.1f in %u Ticks",
                    static_cast<double>(dx), static_cast<double>(dz), static_cast<double>(std::sqrt(dx * dx + dz * dz)),
                    kMeWalkTicks);
                c.note(buf);
            }
            me_note_link(c, "Ende");
            me_note_hearts(c, "Herzen am Ende");
            me_note_inventory(c, "Inventar am Ende");
            c.note("timeline complete: 20 s laufen nach der Landung");
            return true;
        }
    }
    return false;
}

const kff::autotest::Scenario kMensch = {
    "mensch",
    "Ruf als Mensch, Ritt im Griff, Hoehenhalt, Kreise links und rechts, B zum Absteigen, Landung, 20 s laufen",
    me_tick,
};
#endif  // KFF_DEV_AUTOTEST

class MenschModule final : public kff::Module {
public:
    const char* name() const override {
        return "mensch";
    }

    ModResult install(kff::Registry& reg) override {
        ModResult r = reg.addPost(kff::Hook::LinkExecute, &on_link_execute_post, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        r = reg.addPost(kff::Hook::KagoExecute, &on_kago_execute_post, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        r = reg.addPost(kff::Hook::LinkPosMove, &on_link_pos_move_post, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        r = reg.addPre(kff::Hook::LinkSetMatrix, &on_link_set_matrix_pre, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        r = reg.addPost(kff::Hook::LinkSetBodyPart, &on_link_body_part_post, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        r = reg.addPre(kff::Hook::LinkSetArmMatrix, &on_link_arm_pre, kff::prio::kMensch);
        if (r != MOD_OK) {
            return r;
        }
        return reg.addPre(kff::Hook::LinkFreeGrabItem, &on_link_free_grab_pre, kff::prio::kMensch);
    }

    bool onHumanCall(kff::Session& s, daAlink_c* link) override {
        return human_call(s, link);
    }

    void onRideStart(kff::Session& s, daKago_c* bird) override {
        (void)bird;
        if (!s.rideGrip) {
            return;  // Wolf-Ritt im Cargo: nichts zu tun
        }
        if (daAlink_c* link = kff::liveLink()) {
            begin_grip(link);
        }
    }

    void onFrameLink(kff::Session& s, daAlink_c* link) override {
        watch_grip(s, link);
    }

    void onRideEnd(kff::Session& s, daKago_c* bird, kff::EndReason why) override {
        (void)s;
        (void)bird;
        if (g_grip == Grip::Off && g_niId == kff::kNoProc) {
            return;  // kein Griff in diesem Ritt
        }
        end_grip(kff::liveLink(), why);
    }

#if defined(KFF_DEV_AUTOTEST)
    const kff::autotest::Scenario* autotestScenario() const override {
        return &kMensch;
    }
#endif

    void onShutdown(kff::Session& s) override {
        (void)s;
        if (g_grip != Grip::Off || g_niId != kff::kNoProc) {
            end_grip(kff::liveLink(), kff::EndReason::Shutdown);
        }
        kff::logInfo("mensch: Entladen (Rufe %u, Griffe %u, Enden %u, Loslassen blockiert %u, Repins %u)", g_calls, g_starts,
            g_ends, g_releaseBlocked, g_repins);
    }
};

MenschModule g_module;

}  // namespace

namespace kff::modules {

Module& mensch() {
    return g_module;
}

}  // namespace kff::modules
