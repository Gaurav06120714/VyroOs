#include "apps.h"

extern void launcher_set_callback(void (*cb)(const char*));

void apps_register_all() {
    app_register(&APP_FILES);
    app_register(&APP_TERMINAL);
    app_register(&APP_SETTINGS);
    app_register(&APP_BROWSER2);
    app_register(&APP_LAUNCHER);
    app_register(&APP_TEXTEDIT);
    app_register(&APP_CALC);
    app_register(&APP_CLOCK);
    app_register(&APP_TASKMGR);
    app_register(&APP_NOTECENTER);
    app_register(&APP_CONTROL);
    app_register(&APP_PKGSTORE);
}
