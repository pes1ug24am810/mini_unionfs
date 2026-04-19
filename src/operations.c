#include "../include/unionfs.h"


/* Ensure parent directory exists in upper (mkdir -p for one level) */
static void ensure_parent_dir(const char *path)
{
    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH);
    temp[MAX_PATH - 1] = '\0';

    char *p = strrchr(temp, '/');
    if (!p || p == temp)
        return;

    *p = '\0';

    char full[MAX_PATH];
    build_path(full, STATE->upper, temp);

    mkdir(full, 0755);  // simple, non-recursive
}


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

    /* Track names already added (from upper) */
    char names[1024][256];
    int count = 0;

    /* ---------- READ UPPER ---------- */
    dir = opendir(upper);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {

            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;

            /* Skip whiteout files themselves */
            if (!strncmp(entry->d_name, ".wh.", 4))
                continue;

            filler(buf, entry->d_name, NULL, 0, 0);

            /* Save name to avoid duplicates later */
            strncpy(names[count], entry->d_name, 255);
            names[count][255] = '\0';
            count++;
        }
        closedir(dir);
    }

    /* ---------- READ LOWER ---------- */
    dir = opendir(lower);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {

            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;

            /* Skip if already in upper */
            int found = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(names[i], entry->d_name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found)
                continue;

            /* Build full path for whiteout check */
            char fullpath[MAX_PATH];
            snprintf(fullpath, MAX_PATH, "%s/%s", path, entry->d_name);

            if (is_whiteouted(fullpath))
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
    char whiteout[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);
    build_whiteout(whiteout, path);

    /* 🔴 Remove whiteout if exists */
    unlink(whiteout);

    /* 🔧 Ensure parent directory exists */
    ensure_parent_dir(path);

    /* 🧠 Copy-on-Write */
    if (access(upper, F_OK) != 0) {

        if (access(lower, F_OK) == 0) {
            int res = copy_file(lower, upper);
            if (res != 0)
                return res;
        } else {
            int fd = creat(upper, 0644);
            if (fd == -1)
                return -errno;
            close(fd);
        }
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
    char whiteout[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_whiteout(whiteout, path);

    /* 🔴 Remove whiteout if exists */
    unlink(whiteout);

    /* 🔧 Ensure parent directory exists */
    ensure_parent_dir(path);

    int fd = creat(upper, mode);
    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}


static int unionfs_unlink(const char *path)
{
    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);
    build_whiteout(whiteout, path);

    int in_upper = (access(upper, F_OK) == 0);
    int in_lower = (access(lower, F_OK) == 0);

    if (!in_upper && !in_lower)
        return -ENOENT;
    
    ensure_parent_dir(path);

    /* Delete from upper if present */
    if (in_upper) {
        if (unlink(upper) == -1)
            return -errno;
    }

    /* If exists in lower → create whiteout */
    if (in_lower) {
        FILE *f = fopen(whiteout, "w");
        if (!f)
            return -errno;
        fclose(f);
    }

    return 0;
}


static int unionfs_mkdir(
    const char *path,
    mode_t mode)
{
    char upper[MAX_PATH];

    build_path(upper, STATE->upper, path);

    /* Remove whiteout if exists */
    char whiteout[MAX_PATH];
    build_whiteout(whiteout, path);
    unlink(whiteout);

    ensure_parent_dir(path);

    if (mkdir(upper, mode) == -1)
        return -errno;

    return 0;
}


static int unionfs_rmdir(const char *path)
{
    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);
    ensure_parent_dir(path);
    build_whiteout(whiteout, path);

    int in_upper = (access(upper, F_OK) == 0);
    int in_lower = (access(lower, F_OK) == 0);

    if (!in_upper && !in_lower)
        return -ENOENT;

    /* 🔴 Check if directory is empty in merged view */
    DIR *dir;
    struct dirent *entry;

    /* Check upper */
    dir = opendir(upper);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") &&
                strcmp(entry->d_name, "..")) {
                closedir(dir);
                return -ENOTEMPTY;
            }
        }
        closedir(dir);
    }

    /* Check lower (excluding whiteouts) */
    dir = opendir(lower);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {

            if (!strcmp(entry->d_name, ".") ||
                !strcmp(entry->d_name, ".."))
                continue;

            /* Skip if hidden by whiteout */
            char fullpath[MAX_PATH];
            snprintf(fullpath, MAX_PATH, "%s/%s", path, entry->d_name);

            if (is_whiteouted(fullpath))
                continue;

            closedir(dir);
            return -ENOTEMPTY;
        }
        closedir(dir);
    }

    /* Delete upper if exists */
    if (in_upper) {
        if (rmdir(upper) == -1)
            return -errno;
    }

    /* If exists in lower → create whiteout */
    if (in_lower) {
        FILE *f = fopen(whiteout, "w");
        if (!f)
            return -errno;
        fclose(f);
    }

    return 0;
}


/* Register operations */
struct fuse_operations ops = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .read    = unionfs_read,
    .write   = unionfs_write,
    .create  = unionfs_create,
    .unlink  = unionfs_unlink,
    .mkdir   = unionfs_mkdir,
    .rmdir   = unionfs_rmdir,
};
