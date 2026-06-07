#include "vfs.h"
#include "heap.h"

static vfs_node_t* root = 0;

// ─────────────────────────────────────────────────
// String helpers (freestanding)
// ─────────────────────────────────────────────────
static void vstrcpy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int vstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static int vstrlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

// ─────────────────────────────────────────────────
// vfs_init: create root directory and seed some files
// ─────────────────────────────────────────────────
void vfs_init() {
    root = (vfs_node_t*) kmalloc_zero(sizeof(vfs_node_t));
    vstrcpy(root->name, "/", VFS_NAME_MAX);
    root->type   = VFS_DIRECTORY;
    root->parent = root;   // Root's parent is itself

    // Seed an example tree
    vfs_node_t* home = vfs_create(root, "home", VFS_DIRECTORY);
    vfs_create(root, "bin", VFS_DIRECTORY);

    vfs_node_t* readme = vfs_create(root, "readme.txt", VFS_FILE);
    vfs_write(readme, "Welcome to VyFS - the Vyro OS filesystem!");

    vfs_node_t* user = vfs_create(home, "user", VFS_DIRECTORY);
    vfs_node_t* notes = vfs_create(user, "notes.txt", VFS_FILE);
    vfs_write(notes, "Build an OS from scratch. Done!");
}

vfs_node_t* vfs_root() {
    return root;
}

// ─────────────────────────────────────────────────
// vfs_create: add a child node to a directory
// ─────────────────────────────────────────────────
vfs_node_t* vfs_create(vfs_node_t* parent, const char* name, uint8_t type) {
    if (!parent || parent->type != VFS_DIRECTORY) return 0;
    if (vfs_find(parent, name)) return 0;  // Already exists

    vfs_node_t* node = (vfs_node_t*) kmalloc_zero(sizeof(vfs_node_t));
    if (!node) return 0;

    vstrcpy(node->name, name, VFS_NAME_MAX);
    node->type   = type;
    node->parent = parent;
    node->mode   = (type == VFS_DIRECTORY) ? 0755 : 0644;
    node->uid    = 0;
    node->symlink_target = 0;

    // Append to parent's child list
    if (!parent->first_child) {
        parent->first_child = node;
    } else {
        vfs_node_t* cur = parent->first_child;
        while (cur->next_sibling) cur = cur->next_sibling;
        cur->next_sibling = node;
    }
    return node;
}

// ─────────────────────────────────────────────────
// vfs_find: look up a child by name in a directory
// ─────────────────────────────────────────────────
vfs_node_t* vfs_find(vfs_node_t* dir, const char* name) {
    if (!dir || dir->type != VFS_DIRECTORY) return 0;

    // Special names
    if (vstrcmp(name, ".") == 0)  return dir;
    if (vstrcmp(name, "..") == 0) return dir->parent;

    vfs_node_t* cur = dir->first_child;
    while (cur) {
        if (vstrcmp(cur->name, name) == 0) return cur;
        cur = cur->next_sibling;
    }
    return 0;
}

// ─────────────────────────────────────────────────
// vfs_remove: delete a child by name
// ─────────────────────────────────────────────────
int vfs_remove(vfs_node_t* dir, const char* name) {
    if (!dir || dir->type != VFS_DIRECTORY) return 0;

    vfs_node_t* cur  = dir->first_child;
    vfs_node_t* prev = 0;

    while (cur) {
        if (vstrcmp(cur->name, name) == 0) {
            // Unlink from sibling list
            if (prev) prev->next_sibling = cur->next_sibling;
            else      dir->first_child   = cur->next_sibling;

            if (cur->content) kfree(cur->content);
            kfree(cur);
            return 1;
        }
        prev = cur;
        cur  = cur->next_sibling;
    }
    return 0;
}

// ─────────────────────────────────────────────────
// vfs_write: set file content (overwrites)
// ─────────────────────────────────────────────────
int vfs_write(vfs_node_t* file, const char* data) {
    if (!file || file->type != VFS_FILE) return 0;

    if (file->content) kfree(file->content);

    int len = vstrlen(data);
    file->content = (char*) kmalloc(len + 1);
    if (!file->content) { file->size = 0; return 0; }

    vstrcpy(file->content, data, len + 1);
    file->size = len;
    return len;
}

// ─────────────────────────────────────────────────
// vfs_read: get file content
// ─────────────────────────────────────────────────
const char* vfs_read(vfs_node_t* file) {
    if (!file || file->type != VFS_FILE) return 0;
    return file->content ? file->content : "";
}

// ─────────────────────────────────────────────────
// vfs_full_path: build absolute path string for a node
// ─────────────────────────────────────────────────
void vfs_full_path(vfs_node_t* node, char* buf, int buf_size) {
    // vC.6.10.6: guard against NULL node and corrupted parent chains.
    // Without these the function GP-faults during boot when shell_init
    // calls print_prompt before fs_ensure has chance to defer to root,
    // or when an app calls vfs_full_path with a node whose parent has
    // been freed / not yet wired up.
    if (!node || !root) {
        vstrcpy(buf, "/", buf_size);
        return;
    }
    if (node == root) {
        vstrcpy(buf, "/", buf_size);
        return;
    }

    // Walk up to root collecting names, then build forward
    char temp[256];
    int  pos = 0;
    temp[0] = '\0';

    // Build reversed path segments. Loop terminates on: reached root,
    // hit a NULL parent (defensive — should not happen but cheaper than
    // a GPF if it does), or depth cap hit.
    vfs_node_t* stack[32];
    int depth = 0;
    vfs_node_t* cur = node;
    while (cur && cur != root && depth < 32) {
        stack[depth++] = cur;
        cur = cur->parent;
    }

    for (int i = depth - 1; i >= 0; i--) {
        temp[pos++] = '/';
        const char* n = stack[i]->name;
        for (int j = 0; n[j] && pos < 255; j++) temp[pos++] = n[j];
    }
    temp[pos] = '\0';

    vstrcpy(buf, temp, buf_size);
}

void vfs_chmod(vfs_node_t* node, uint16_t mode) {
    if (node) node->mode = mode & 07777;
}

vfs_node_t* vfs_symlink(vfs_node_t* parent, const char* name, const char* target) {
    vfs_node_t* n = vfs_create(parent, name, VFS_SYMLINK);
    if (!n || !target) return n;
    // Allocate + copy target path
    uint32_t len = 0; while (target[len]) len++;
    char* buf = (char*)kmalloc_zero(len + 1);
    if (!buf) return n;
    for (uint32_t i = 0; i < len; i++) buf[i] = target[i];
    buf[len] = 0;
    n->symlink_target = buf;
    n->size = len;
    return n;
}
