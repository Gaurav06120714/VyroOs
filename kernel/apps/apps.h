#ifndef APPS_H
#define APPS_H

// Forward declarations for built-in apps.
// Each provides a render(app_ctx_t*) function.

#include "../app.h"

extern const app_def_t APP_SETTINGS;
extern const app_def_t APP_FILES;
extern const app_def_t APP_TERMINAL;
extern const app_def_t APP_TEXTEDIT;
extern const app_def_t APP_CALC;
extern const app_def_t APP_CLOCK;
extern const app_def_t APP_TASKMGR;
extern const app_def_t APP_LAUNCHER;
extern const app_def_t APP_NOTECENTER;
extern const app_def_t APP_CONTROL;
extern const app_def_t APP_BROWSER2;
extern const app_def_t APP_PKGSTORE;

void apps_register_all();

#endif
