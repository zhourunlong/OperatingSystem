#include "stats.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "path.h"
#include "blockio.h"

#include <stdlib.h>
#include <string.h>

int o_statfs(const char* path, struct statvfs* stbuf) {
// std::lock_guard <std::mutex> guard(global_lock);
    opt_lock_holder zhymoyu;
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "STATFS, %s, %p\n", resolve_prefix(path).c_str(), stbuf);
    
    int inum;
    int locate_err = locate(path, inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the directory (error #%d).\n", locate_err);
        return locate_err;
    }

    std::set <int> get_inodes;
    get_inodes.insert(inum);

    /* The inode-level fine-grained lock is added by a lock_guard. */
    std::lock_guard <std::mutex> guard(inode_lock[inum]);
    /* This will be automatically released on each exit path. */

    stbuf->f_bsize = stbuf->f_frsize = BLOCK_SIZE;
    stbuf->f_blocks = 0;
    stbuf->f_bfree = stbuf->f_bavail = BLOCKS_IN_SEGMENT * TOT_SEGMENTS - stbuf->f_blocks;
    stbuf->f_files = count_inode;
    stbuf->f_ffree = stbuf->f_favail = MAX_NUM_INODE - count_inode - 1;
    stbuf->f_fsid = 0;
    stbuf->f_flag = 0;
    stbuf->f_namemax = MAX_FILENAME_LEN - 1;

    return 0;
}

int o_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info *fi) {
// std::lock_guard <std::mutex> guard(global_lock);
    opt_lock_holder zhymoyu;
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UTIMENS, %s, %p, %p\n",
               resolve_prefix(path).c_str(), &ts, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full: please expand the disk size.\n* Garbage collection fails because it cannot release any blocks.\n");
        logger(WARN, "====> Cannot proceed to update timestamps.\n");
        return -ENOSPC;
    }
    
    int inum;
    int locate_err = locate(path, inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the directory (error #%d).\n", locate_err);
        return locate_err;
    }

    std::set <int> get_inodes;
    get_inodes.insert(inum);

    /* The inode-level fine-grained lock is added by a lock_guard. */
    std::lock_guard <std::mutex> guard(inode_lock[inum]);
    /* This will be automatically released on each exit path. */

    inode* block_inode;
    get_inode_from_inum(block_inode, inum);

    block_inode->atime = ts[0];
    block_inode->mtime = ts[1];

    new_inode_block(block_inode);

    return 0;
}
