#include "stats.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "path.h"
#include "blockio.h"

#include <stdlib.h>
#include <string.h>

int o_statfs(const char* path, struct statvfs* stbuf) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "STATFS, %s, %p\n", resolve_prefix(path).c_str(), stbuf);
    
    int inum;
    int locate_err = locate(path, inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the directory (error #%d).\n", locate_err);
        return locate_err;
    }

    /*
    inode block_inode;
    get_inode_from_inum(&block_inode, inum);
    */

    stbuf->f_bsize = stbuf->f_frsize = BLOCK_SIZE;
    stbuf->f_blocks = cur_segment * BLOCKS_IN_SEGMENT + cur_block;
    stbuf->f_bfree = stbuf->f_bavail = BLOCKS_IN_SEGMENT * TOT_SEGMENTS - stbuf->f_blocks;
    stbuf->f_files = count_inode;
    stbuf->f_ffree = stbuf->f_favail = MAX_NUM_INODE - count_inode - 1;
    stbuf->f_fsid = 0;
    stbuf->f_flag = 0;
    stbuf->f_namemax = MAX_FILENAME_LEN - 1;

    return 0;
}

int o_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UTIMENS, %s, %p, %p\n",
               resolve_prefix(path).c_str(), &ts, fi);
    
    int inum;
    int locate_err = locate(path, inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the directory (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    int ginode_err = get_inode_from_inum(&block_inode, inum);
    if (ginode_err != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission error when loading inode.\n");
        return ginode_err;
    }

    block_inode.atime = ts[0];
    block_inode.mtime = ts[1];

    new_inode_block(&block_inode);

    return 0;
}
