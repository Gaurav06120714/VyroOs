#ifndef APPS_H
#define APPS_H

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
extern const app_def_t APP_NOTES;
extern const app_def_t APP_CALENDAR;
extern const app_def_t APP_NETWORK;
extern const app_def_t APP_DISK;

void apps_register_all();

#endif
