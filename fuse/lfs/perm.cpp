#include "perm.h"

#include "logger.h"
#include "print.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

int o_chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
    // Never try to access "fi": it causes segmentation fault.
// std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CHMOD, %s, %d, %p\n",
               resolve_prefix(path).c_str(), mode, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full: please expand the disk size.\n* Garbage collection fails because it cannot release any blocks.\n");
        logger(WARN, "====> Cannot proceed to change permission of the file / directory.\n");
        return -ENOSPC;
    }
    
    int fh;
    int locate_err = locate(path, fh);
    if (locate_err != 0) {
        if (ERROR_PERMCPP)
            logger(ERROR, "[ERROR] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }
    std::set <int> get_inodes;
    get_inodes.insert(fh);

    std::lock_guard <std::mutex> guard(inode_lock[fh]);

    inode* block_inode;
    get_inode_from_inum(block_inode, fh);
    block_inode->permission = mode & 0777;
    clock_gettime(CLOCK_REALTIME, &block_inode->ctime);
    new_inode_block(block_inode);

    return 0;
}

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) { //libreoffice may invoke the func and set uid -1
    // Never try to access "fi": it causes segmentation fault.
// std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CHOWN, %s, %d, %d, %p\n",
               resolve_prefix(path).c_str(), uid, gid, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full: please expand the disk size.\n* Garbage collection fails because it cannot release any blocks.\n");
        logger(WARN, "====> Cannot proceed to change the owner of the file / directory.\n");
        return -ENOSPC;
    }

    int fh;
    int locate_err = locate(path, fh);
    if (locate_err != 0) {
        if (ERROR_PERMCPP)
            logger(ERROR, "[ERROR] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }

    std::set <int> get_inodes;
    get_inodes.insert(fh);

    std::lock_guard <std::mutex> guard(inode_lock[fh]);

    inode* block_inode;
    get_inode_from_inum(block_inode, fh);
    if (uid != -1)
        block_inode->perm_uid = uid;
    if (gid != -1)
        block_inode->perm_gid = gid;
    clock_gettime(CLOCK_REALTIME, &block_inode->ctime);
    new_inode_block(block_inode);

    return 0;
}
