#include "opendir.h"
#include "prefix.h"

int o_opendir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}
