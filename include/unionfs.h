#ifndef UNIONFS_H
#define UNIONFS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096

/* Global state */
struct unionfs_state {
    char lower[MAX_PATH];
    char upper[MAX_PATH];
};

/* Access state anywhere */
#define STATE ((struct unionfs_state *) fuse_get_context()->private_data)

/* Helper function declarations */
void build_path(char *dest, const char *base, const char *path);
void build_whiteout(char *dest, const char *path);
int is_whiteouted(const char *path);
int resolve_path(const char *path, char *resolved);
int copy_file(const char *src, const char *dest);

/* FUSE operations */
extern struct fuse_operations ops;

#endif
