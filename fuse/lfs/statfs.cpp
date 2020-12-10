#include "statfs.h"

#include "logger.h"
#include "utility.h"
#include "path.h"

int o_statfs(const char* path, struct statvfs* stbuf) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "STATFS, %s, %p\n", resolve_prefix(path), stbuf);
    
    return 0;
}
