#ifndef VFS_H
#define VFS_H

#include "../include/types.h"

#define VFS_NAME_MAX   64
#define VFS_FILE       0
#define VFS_DIRECTORY  1

// ─────────────────────────────────────────────────
// VyFS node — one file or directory
// Tree structure: first_child + next_sibling
// ─────────────────────────────────────────────────
typedef struct vfs_node {
    char              name[VFS_NAME_MAX];
    uint8_t           type;          // VFS_FILE or VFS_DIRECTORY
    struct vfs_node*  parent;
    struct vfs_node*  first_child;   // For directories
    struct vfs_node*  next_sibling;  // Next entry in parent dir
    char*             content;       // File data (NULL for dirs)
    uint32_t          size;          // File size in bytes
    uint16_t          mode;          // Unix-style permissions (e.g. 0755 / 0644)
    uint16_t          uid;           // owning user id (0 = root)
    char*             symlink_target; // non-NULL = symbolic link to this path
} vfs_node_t;

// Symlink type. Treated transparently by vfs_find when symlink_target is set.
#define VFS_SYMLINK    2

void vfs_chmod(vfs_node_t* node, uint16_t mode);
vfs_node_t* vfs_symlink(vfs_node_t* parent, const char* name, const char* target);

void        vfs_init();
vfs_node_t* vfs_root();

// Directory operations
vfs_node_t* vfs_create(vfs_node_t* parent, const char* name, uint8_t type);
vfs_node_t* vfs_find(vfs_node_t* dir, const char* name);
int         vfs_remove(vfs_node_t* dir, const char* name);

// File operations
int         vfs_write(vfs_node_t* file, const char* data);
const char* vfs_read(vfs_node_t* file);

// Path helpers
void        vfs_full_path(vfs_node_t* node, char* buf, int buf_size);

#endif
