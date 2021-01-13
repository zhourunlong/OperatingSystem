#include "buffer.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"
#include "path.h"
#include "wbcache.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <mutex>

int o_flush(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FLUSH, %s, %p\n", resolve_prefix(path).c_str(), fi);
    return 0;
}

/* Synchronize manually by writing the current segment into disk file and generate a checkpoint. */
void manually_synchronize() {
    // Currently flush the whole segment buffer to disk (the same as destroy()).
    // Only allow flushing when there is not an on-going GC.
    acquire_segment_lock();
        if (!is_doing_gc) {
            add_segbuf_metadata();
            if (USE_CACHE)
                write_segment_through_cache(segment_buffer, cur_segment);
            else
                write_segment(segment_buffer, cur_segment);
            segment_bitmap[cur_segment] = 1;
            generate_checkpoint();

            // Update checkpoint time.
            struct timespec cur_time;
            clock_gettime(CLOCK_REALTIME, &cur_time);
            last_ckpt_update_time = cur_time;
        }
    release_segment_lock();
}

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNC, %s, %d, %p\n",
               resolve_prefix(path).c_str(), isdatasync, fi);
    
    manually_synchronize();
    
    return 0;
}

int o_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
               resolve_prefix(path).c_str(), isdatasync, fi);
    
    manually_synchronize();
    
    return 0;
}