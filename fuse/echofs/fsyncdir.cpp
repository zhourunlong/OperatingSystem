#include "fsyncdir.h"
#include "prefix.h"

int o_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi) {
    logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
        resolve_prefix(path), isdatasync, fi);
    return 0;
}
