#include "statfs.h"
#include "prefix.h"

int o_statfs(const char* path, struct statvfs* stbuf) {
    logger(DEBUG, "STATFS, %s, %p\n", resolve_prefix(path), stbuf);
    return 0;
}
