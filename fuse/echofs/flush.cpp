#include "flush.h"
#include "prefix.h"

int o_flush(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "FLUSH, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}
