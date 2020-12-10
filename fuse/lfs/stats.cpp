#include "stats.h"

#include "logger.h"
#include "utility.h"
#include "path.h"

int o_statfs(const char* path, struct statvfs* stbuf) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "STATFS, %s, %p\n", resolve_prefix(path), stbuf);
    
    return 0;
}

int o_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UTIMENS, %s, %p, %p\n",
               resolve_prefix(path), &ts, fi);
    
    return 0;
}
