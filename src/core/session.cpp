#include "d/dolzel_rel.h"

#include "core/kff/session.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

namespace kff {
namespace {

Session g_session;

}  // namespace

Session& session() {
    return g_session;
}

const char* phaseName(Phase phase) {
    switch (phase) {
    case Phase::Idle:
        return "Idle";
    case Phase::Summoning:
        return "Summoning";
    case Phase::Transforming:
        return "Transforming";
    case Phase::Riding:
        return "Riding";
    case Phase::Dismounting:
        return "Dismounting";
    }
    return "?";
}

const char* endReasonName(EndReason reason) {
    switch (reason) {
    case EndReason::DismountButton:
        return "DismountButton";
    case EndReason::RiderLost:
        return "RiderLost";
    case EndReason::Event:
        return "Event";
    case EndReason::LinkChanged:
        return "LinkChanged";
    case EndReason::DeleteRequested:
        return "DeleteRequested";
    case EndReason::BirdGone:
        return "BirdGone";
    case EndReason::Shutdown:
        return "Shutdown";
    case EndReason::Death:
        return "Death";
    case EndReason::Menu:
        return "Menu";
    case EndReason::Transform:
        return "Transform";
    case EndReason::Stuck:
        return "Stuck";
    case EndReason::Reset:
        return "Reset";
    case EndReason::Warp:
        return "Warp";
    }
    return "?";
}

daAlink_c* liveLink() {
    const Session& s = g_session;
    if (s.linkId == kNoProc) {
        return nullptr;
    }
    fopAc_ac_c* actor = nullptr;
    if (fopAcM_SearchByID(s.linkId, &actor) == 0 || actor == nullptr) {
        return nullptr;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr || static_cast<fopAc_ac_c*>(link) != actor) {
        return nullptr;
    }
    return link;
}

daKago_c* liveBird() {
    const Session& s = g_session;
    if (s.birdId == kNoProc) {
        return nullptr;
    }
    fopAc_ac_c* actor = nullptr;
    if (fopAcM_SearchByID(s.birdId, &actor) == 0 || actor == nullptr) {
        return nullptr;
    }
    if (fopAcM_GetName(actor) != fpcNm_KAGO_e) {
        return nullptr;
    }
    return static_cast<daKago_c*>(actor);
}

bool isOwnBird(daKago_c* k) {
    const Session& s = g_session;
    return k != nullptr && s.birdId != kNoProc && fopAcM_GetID(k) == s.birdId &&
           k->mType == kFriendlyKagoType;
}

bool isRiding() {
    return g_session.phase == Phase::Riding;
}

// N1: abfliegender eigener Vogel. Der Landeanflug (landFrames > 0) zaehlt nicht dazu, dort bleibt der Ruf gesperrt.
bool birdDeparting(const Session& s) {
    return s.birdId != kNoProc && (s.phase == Phase::Dismounting || s.deleteRequested) && s.landFrames == 0;
}

}  // namespace kff
