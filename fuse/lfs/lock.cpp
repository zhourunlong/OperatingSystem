#include "lock.h"

#include "logger.h"
#include "utility.h"
#include "path.h"

#include <unistd.h>
#include <mutex>

// A universal system lock used temporarily.
// Should possibly be replaced with an array of mutex locks,
// or even reader-writer locks for better performance.
std::mutex sys_lock;

int o_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* locks) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "LOCK, %s, %p, %d, %p\n",
               resolve_prefix(path), fi, cmd, locks);

    return 0;
}
