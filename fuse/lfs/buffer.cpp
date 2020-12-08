#include "buffer.h"

#include "logger.h"
#include "prefix.h"

int o_flush(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "FLUSH, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    logger(DEBUG, "FSYNC, %s, %d, %p\n",
        resolve_prefix(path), isdatasync, fi);
    return 0;
}

int o_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi) {
    logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
        resolve_prefix(path), isdatasync, fi);
    return 0;
}