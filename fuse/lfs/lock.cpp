#include "lock.h"

#include "logger.h"
#include "utility.h"
#include "path.h"

#include <unistd.h>

int o_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* locks) {
    // sleep(2);

    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "LOCK, %s, %p, %d, %p\n",
               resolve_prefix(path), fi, cmd, locks);
    
    return 0;
}
