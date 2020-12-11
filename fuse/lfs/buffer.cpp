#include "buffer.h"

#include "logger.h"
#include "utility.h"
#include "path.h"

#include <unistd.h>

int o_flush(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FLUSH, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNC, %s, %d, %p\n",
               resolve_prefix(path), isdatasync, fi);
    return 0;
}

int o_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
               resolve_prefix(path), isdatasync, fi);
    return 0;
}