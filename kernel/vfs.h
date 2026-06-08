#ifndef VFS_H
#define VFS_H

#include "../include/types.h"

#define VFS_NAME_MAX   64
#define VFS_FILE       0
#define VFS_DIRECTORY  1

typedef struct vfs_node {
    char              name[VFS_NAME_MAX];
    uint8_t           type;
    struct vfs_node*  parent;
    struct vfs_node*  first_child;
    struct vfs_node*  next_sibling;
    char*             content;
    uint32_t          size;
    uint16_t          mode;
    uint16_t          uid;
    char*             symlink_target;
} vfs_node_t;

#define VFS_SYMLINK    2

void vfs_chmod(vfs_node_t* node, uint16_t mode);
vfs_node_t* vfs_symlink(vfs_node_t* parent, const char* name, const char* target);

void        vfs_init();
vfs_node_t* vfs_root();

vfs_node_t* vfs_create(vfs_node_t* parent, const char* name, uint8_t type);
vfs_node_t* vfs_find(vfs_node_t* dir, const char* name);
int         vfs_remove(vfs_node_t* dir, const char* name);

int         vfs_write(vfs_node_t* file, const char* data);
const char* vfs_read(vfs_node_t* file);

void        vfs_full_path(vfs_node_t* node, char* buf, int buf_size);

#endif
