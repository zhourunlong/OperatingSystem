#include "fsync.h"
#include "prefix.h"

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    logger(DEBUG, "FSYNC, %s, %d, %p\n",
        resolve_prefix(path), isdatasync, fi);
    return 0;
}
