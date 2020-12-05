#include "read.h"
#include "prefix.h"

int o_read(
    const char* path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info* fi
) {
    logger(DEBUG, "READ, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);
    return 0;
}
