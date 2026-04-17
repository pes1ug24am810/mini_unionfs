#include "../include/unionfs.h"

/* Build full path */
void build_path(char *dest, const char *base, const char *path)
{
    snprintf(dest, MAX_PATH, "%s%s", base, path);
}

/* Build whiteout file path */
void build_whiteout(char *dest, const char *path)
{
    char temp[MAX_PATH];
    strcpy(temp, path);

    char *last = strrchr(temp, '/');

    /* file in root */
    if (last == temp) {
        snprintf(dest, MAX_PATH,
                 "%s/.wh.%s",
                 STATE->upper,
                 path + 1);
        return;
    }

    *last = '\0';

    snprintf(dest, MAX_PATH,
             "%s%s/.wh.%s",
             STATE->upper,
             temp,
             last + 1);
}

/* Check if whiteout exists */
int is_whiteouted(const char *path)
{
    char whiteout[MAX_PATH];

    build_whiteout(whiteout, path);

    return access(whiteout, F_OK) == 0;
}

/* Resolve file path:
   upper first, then lower
*/
int resolve_path(const char *path, char *resolved)
{
    char upper[MAX_PATH];
    char lower[MAX_PATH];

    if (is_whiteouted(path))
        return -ENOENT;

    build_path(upper, STATE->upper, path);
    build_path(lower, STATE->lower, path);

    if (access(upper, F_OK) == 0) {
        strcpy(resolved, upper);
        return 0;
    }

    if (access(lower, F_OK) == 0) {
        strcpy(resolved, lower);
        return 0;
    }

    return -ENOENT;
}

/* Copy file for Copy-on-Write */
int copy_file(const char *src, const char *dest)
{
    FILE *s = fopen(src, "rb");
    if (!s)
        return -errno;

    FILE *d = fopen(dest, "wb");
    if (!d) {
        fclose(s);
        return -errno;
    }

    char buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), s)) > 0)
        fwrite(buf, 1, n, d);

    fclose(s);
    fclose(d);

    return 0;
}
