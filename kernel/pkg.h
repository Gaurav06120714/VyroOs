#ifndef PKG_H
#define PKG_H

#include "../include/types.h"

#define PKG_NAME_MAX  24
#define PKG_MAX_DEPS  4
#define PKG_REPO_MAX  16

typedef struct {
    const char* name;
    const char* version;
    const char* desc;
    const char* deps[PKG_MAX_DEPS];   // NULL-terminated list
    uint8_t     installed;
} package_t;

void pkg_init();
int  pkg_install(const char* name);   // returns # newly installed, -1 not found
int  pkg_remove(const char* name);    // returns 0 ok, -1 not found/installed
package_t* pkg_repo(int* count);
package_t* pkg_find(const char* name);

#endif
