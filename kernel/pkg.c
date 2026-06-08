#include "pkg.h"
#include "vfs.h"
#include "../drivers/screen.h"

static package_t repo[PKG_REPO_MAX] = {
    { "libc",     "1.0", "Vyro C runtime library",        {0},                  0 },
    { "libtext",  "1.2", "Text rendering library",        {"libc", 0},          0 },
    { "editor",   "0.9", "Vyro text editor",              {"libtext", 0},       0 },
    { "vinet",    "2.1", "Networking utilities",          {"libc", 0},          0 },
    { "vsh",      "1.0", "Extended shell",                {"libc", 0},          0 },
    { "games",    "0.3", "Terminal games pack",           {"libtext", "vsh", 0},0 },
    { "vdoc",     "1.1", "Documentation viewer",          {"libtext", 0},       0 },
    { "coreutils","1.5", "Core command-line utilities",   {"libc", 0},          0 },
};
static int repo_count = 8;

static int pkg_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

void pkg_init() {
    for (int i = 0; i < repo_count; i++) repo[i].installed = 0;
}

package_t* pkg_repo(int* count) { *count = repo_count; return repo; }

package_t* pkg_find(const char* name) {
    for (int i = 0; i < repo_count; i++)
        if (pkg_strcmp(repo[i].name, name) == 0) return &repo[i];
    return 0;
}

static int install_rec(package_t* p) {
    if (p->installed) return 0;

    int count = 0;


    for (int i = 0; i < PKG_MAX_DEPS && p->deps[i]; i++) {
        package_t* dep = pkg_find(p->deps[i]);
        if (dep) count += install_rec(dep);
    }


    p->installed = 1;
    count++;


    print_color("  installing ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print(p->name);
    print(" v");
    print(p->version);
    print_char('\n');


    vfs_node_t* root = vfs_root();
    vfs_node_t* bin  = vfs_find(root, "bin");
    if (!bin) bin = vfs_create(root, "bin", VFS_DIRECTORY);
    if (bin) {
        vfs_node_t* f = vfs_find(bin, p->name);
        if (!f) f = vfs_create(bin, p->name, VFS_FILE);
        if (f) vfs_write(f, p->desc);
    }

    return count;
}

int pkg_install(const char* name) {
    package_t* p = pkg_find(name);
    if (!p) return -1;
    return install_rec(p);
}

int pkg_remove(const char* name) {
    package_t* p = pkg_find(name);
    if (!p || !p->installed) return -1;
    p->installed = 0;


    vfs_node_t* bin = vfs_find(vfs_root(), "bin");
    if (bin) vfs_remove(bin, name);
    return 0;
}
