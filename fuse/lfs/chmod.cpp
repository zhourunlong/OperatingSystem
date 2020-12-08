#include "chmod.h"
#include "prefix.h"

int o_chmod(const char* path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CHMOD, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
    return 0;
}
