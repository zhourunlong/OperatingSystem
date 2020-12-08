#include "write.h"
#include "prefix.h"

int o_write(
    const char* path,
    const char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info* fi
) {
    logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);
    return 0;
}
