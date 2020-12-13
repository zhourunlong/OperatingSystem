#include "buffer.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"
#include "path.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <mutex>

int o_flush(const char* path, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FLUSH, %s, %p\n", resolve_prefix(path).c_str(), fi);
    return 0;
}

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNC, %s, %d, %p\n",
               resolve_prefix(path).c_str(), isdatasync, fi);
    
    // Currently flush the whole segment buffer to disk (the same as destroy()).
    add_segbuf_metadata();
    write_segment(segment_buffer, cur_segment);
    segment_bitmap[cur_segment] = 1;
    generate_checkpoint();

    // Update checkpoint time.
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    last_ckpt_update_time = cur_time;
    
    print_inode_table();
    return 0;
}

int o_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
               resolve_prefix(path).c_str(), isdatasync, fi);
    
    // Currently flush the whole segment buffer to disk (the same as destroy()).
    add_segbuf_metadata();
    write_segment(segment_buffer, cur_segment);
    segment_bitmap[cur_segment] = 1;
    generate_checkpoint(); 

    // Update checkpoint time.
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    last_ckpt_update_time = cur_time;
    
    print_inode_table();
    return 0;
}