// Einzige Stelle mit DEFINE_MOD und den Dienst-Importen. Die Dienst-Variablen sind hier definiert,
// alle anderen Dateien kennen sie ueber die SDK-Header (extern).
#include "core/kff/services.hpp"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_OPTIONAL_SERVICE(ConfigService, svc_config);
IMPORT_OPTIONAL_SERVICE(CameraService, svc_camera);
IMPORT_OPTIONAL_SERVICE(UiService, svc_ui);
