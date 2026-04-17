#include "../include/unionfs.h"

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr,
        "Usage: %s <lower_dir> <upper_dir> <mount_point>\n",
        argv[0]);
        return 1;
    }

    struct unionfs_state *state =
        malloc(sizeof(struct unionfs_state));

    if (state == NULL) {
        perror("malloc");
        return 1;
    }

    realpath(argv[1], state->lower);
    realpath(argv[2], state->upper);

    /* FUSE expects program name + mountpoint */
    char *fuse_argv[] = {
        argv[0],
        argv[3],
        NULL
    };

    return fuse_main(2, fuse_argv, &ops, state);
}
