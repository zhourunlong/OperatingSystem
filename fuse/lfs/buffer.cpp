#include "buffer.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"
#include "path.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int o_flush(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND) {
        char* _path = (char*) malloc(mount_dir_len+strlen(path)+4);
        resolve_prefix(path, _path);
        logger(DEBUG, "FLUSH, %s, %p\n", _path, fi);
        free(_path);
    }
    return 0;
}

int o_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND) {
        char* _path = (char*) malloc(mount_dir_len+strlen(path)+4);
        resolve_prefix(path, _path);
        logger(DEBUG, "FSYNC, %s, %d, %p\n",
               _path, isdatasync, fi);
        free(_path);
    }
    
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
    if (DEBUG_PRINT_COMMAND) {
        char* _path = (char*) malloc(mount_dir_len+strlen(path)+4);
        resolve_prefix(path, _path);
        logger(DEBUG, "FSYNCDIR, %s, %d, %p\n",
               _path, isdatasync, fi);
        free(_path);
   }
    
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