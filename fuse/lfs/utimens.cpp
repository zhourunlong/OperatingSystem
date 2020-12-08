#include "utimens.h"

#include "logger.h"
#include "path.h"

int o_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info *fi) {
    logger(DEBUG, "UTIMENS, %s, %p, %p\n",
        resolve_prefix(path), &ts, fi);
    return 0;
}
