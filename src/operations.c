#include "../include/unionfs.h"

/* Get file details */
static int unionfs_getattr(
    const char *path,
    struct stat *stbuf,
    struct fuse_file_info *fi)
{
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char resolved[MAX_PATH];

    int res = resolve_path(path, resolved);
    if (res != 0)
        return res;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}


/* List files */
static int unionfs_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    char upper[MAX_PATH];
    char lower[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);

    DIR *dir;
    struct dirent *entry;

    /* Read upper first */
    dir = opendir(upper);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {

            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;

            if (!strncmp(entry->d_name, ".wh.", 4))
                continue;

            filler(buf, entry->d_name, NULL, 0, 0);
        }
        closedir(dir);
    }

    /* Read lower next */
    dir = opendir(lower);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {

            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;

            filler(buf, entry->d_name, NULL, 0, 0);
        }
        closedir(dir);
    }

    return 0;
}


/* Read file */
static int unionfs_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void) fi;

    char resolved[MAX_PATH];

    int res = resolve_path(path, resolved);
    if (res != 0)
        return res;

    int fd = open(resolved, O_RDONLY);

    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);

    close(fd);

    return res < 0 ? -errno : res;
}


/* Write file */
static int unionfs_write(
    const char *path,
    const char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void) fi;

    char upper[MAX_PATH];
    char lower[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);

    /* Copy-on-Write */
    if (access(upper, F_OK) != 0) {

        if (access(lower, F_OK) == 0)
            copy_file(lower, upper);
        else
            creat(upper, 0644);
    }

    int fd = open(upper, O_WRONLY);

    if (fd == -1)
        return -errno;

    int res = pwrite(fd, buf, size, offset);

    close(fd);

    return res < 0 ? -errno : res;
}


/* Create file */
static int unionfs_create(
    const char *path,
    mode_t mode,
    struct fuse_file_info *fi)
{
    (void) fi;

    char upper[MAX_PATH];

    build_path(upper, STATE->upper, path);

    int fd = creat(upper, mode);

    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}


/* Delete file */
static int unionfs_unlink(const char *path)
{
    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);
    build_whiteout(whiteout, path);

    /* Delete from upper */
    if (access(upper, F_OK) == 0) {
        unlink(upper);
        return 0;
    }

    /* Lower only → create whiteout */
    if (access(lower, F_OK) == 0) {
        FILE *f = fopen(whiteout, "w");
        fclose(f);
        return 0;
    }

    return -ENOENT;
}


/* Register operations */
struct fuse_operations ops = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .read    = unionfs_read,
    .write   = unionfs_write,
    .create  = unionfs_create,
    .unlink  = unionfs_unlink
};
