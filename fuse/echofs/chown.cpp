#include "chown.h"
#include "prefix.h"

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    logger(DEBUG, "CHOWN, %s, %d, %d, %p\n",
        resolve_prefix(path), uid, gid, fi);
    return 0;
}
