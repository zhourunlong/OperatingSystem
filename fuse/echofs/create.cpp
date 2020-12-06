#include "chown.h"
#include "prefix.h"

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CREATE, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
    return 0;
}
