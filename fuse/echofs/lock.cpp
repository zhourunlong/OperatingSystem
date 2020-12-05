#include "lock.h"
#include "prefix.h"

int o_lock(
    const char* path,
    struct fuse_file_info* fi,
    int cmd,
    struct flock* locks
) {
    logger(DEBUG, "LOCK, %s, %p, %d, %p\n",
        resolve_prefix(path), fi, cmd, locks);
    return 0;
}
