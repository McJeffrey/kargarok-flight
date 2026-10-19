#include "d/dolzel_rel.h"

#include "core/kff/fakepath.hpp"

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_kago.h"
#include "d/d_path.h"

namespace kff::fakepath {
namespace {

// Weit unter jedem Boden, damit der Pfad nie das Bodenmaximum gewinnt (MVP-Wert).
constexpr f32 kPathLowY = -100000.0f;
// XZ-Abstand vor und hinter dem Vogel, muss > 0 sein.
constexpr f32 kPathLen = 1000.0f;

struct FakePath {
    dPath path;
    dPnt pnts[3];
};
static_assert(sizeof(FakePath) == 0x3C, "FakePath size");

// Der Ersatzpfad liegt auf dem Heap und wird NIE freigegeben. Der vogel liest mpPath1 auch nach dem Entladen
// der Mod weiter (Vorrecherche und Spaetfund absturz-mppath1): Statischer Speicher der Mod waere dann ungueltig.
// Die Groesse ist fest (60 Byte), das Leck ist bewusst klein und einmalig.
FakePath* g_fake = nullptr;

void init() {
    if (g_fake != nullptr) {
        return;
    }
    g_fake = new FakePath();
    g_fake->path.m_num = 3;  // mindestens 3, sonst kann checkFlySceneChange() am Pfadende feuern
    g_fake->path.m_nextID = 0xFFFF;
    g_fake->path.field_0x4 = 0;
    g_fake->path.m_closed = false;
    g_fake->path.swbit = 0xFF;  // keine Schalterpruefung
    g_fake->path.field_0x7 = 0;
    // m_points ist ein selbstrelativer Offset-Zeiger: Ziel = &m_points + Wert.
    const s32 off = static_cast<s32>(reinterpret_cast<u8*>(&g_fake->pnts[0]) -
                                     reinterpret_cast<u8*>(&g_fake->path.m_points));
    g_fake->path.m_points.value.value = off;
}

}  // namespace

void refresh(daKago_c* bird) {
    if (bird == nullptr) {
        return;
    }
    init();
    const s16 yaw = bird->current.angle.y;
    const f32 dx = cM_ssin(yaw);
    const f32 dz = cM_scos(yaw);
    const f32 px = bird->current.pos.x;
    const f32 pz = bird->current.pos.z;
    g_fake->pnts[0].m_position = Vec{px - dx * kPathLen, kPathLowY, pz - dz * kPathLen};
    g_fake->pnts[1].m_position = Vec{px + dx * kPathLen, kPathLowY, pz + dz * kPathLen};
    g_fake->pnts[2].m_position = Vec{px + dx * kPathLen * 2.0f, kPathLowY, pz + dz * kPathLen * 2.0f};
    bird->mpPath1 = &g_fake->path;
    bird->mPathStep = 1;
    bird->mPathCurrentPointNo = 1;
}

}  // namespace kff::fakepath
