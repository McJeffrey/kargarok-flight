// Host-Dienste und SDK-Header, die alle Dateien des Pakets brauchen.
//
// Die Dienst-Variablen (svc_log, svc_hook, svc_config, svc_camera) werden genau einmal in
// src/core/services.cpp importiert. Die SDK-Header machen sie hier als extern bekannt.
#pragma once

#include "mods/service.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"
