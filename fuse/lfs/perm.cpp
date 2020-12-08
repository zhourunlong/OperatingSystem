#include "perm.h"
#include "logger.h"
#include "prefix.h"

int o_chmod(const char* path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CHMOD, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
    return 0;
}

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    logger(DEBUG, "CHOWN, %s, %d, %d, %p\n",
        resolve_prefix(path), uid, gid, fi);
    return 0;
}
