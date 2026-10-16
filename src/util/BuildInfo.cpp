#include <AppVersion.h>

// Only this translation unit receives the changing version and Git defines.
#ifndef CROSSINK_VERSION
#define CROSSINK_VERSION "dev"
#endif

#ifndef CROSSINK_GIT_SHA
#define CROSSINK_GIT_SHA "unknown"
#endif

#ifndef CROSSINK_GIT_DIRTY
#define CROSSINK_GIT_DIRTY "unknown"
#endif

namespace AppVersion {
const char* version() { return CROSSINK_VERSION; }
// Jester: the user-facing name, e.g. "Jester_v1.0.1" (Settings, boot screen, web).
const char* versionLabel() { return "Jester_v" CROSSINK_VERSION; }
const char* userAgent() { return "CrossInk-ESP32-" CROSSINK_VERSION; }
const char* gitSha() { return CROSSINK_GIT_SHA; }
const char* gitDirtyFlag() { return CROSSINK_GIT_DIRTY; }
}  // namespace AppVersion
