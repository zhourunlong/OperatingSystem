#include "release.h"
#include "prefix.h"

int o_release(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}
