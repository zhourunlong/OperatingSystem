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
    if (DEBUG_PRINT_COMMAND) {
        char* _path = (char*) malloc(mount_dir_len+strlen(path)+4);
        resolve_prefix(path, _path);
        logger(DEBUG, "CHMOD, %s, %d, %p\n",
               _path, mode, fi);
        free(_path);
    }
    
    int fh;
    int locate_err = locate(path, fh);
    if (locate_err != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    get_inode_from_inum(&block_inode, fh);
    block_inode.permission = mode;
    clock_gettime(CLOCK_REALTIME, &block_inode.ctime);
    new_inode_block(&block_inode);

    return 0;
}

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) { //libreoffice may invoke the func and set uid -1
    // Never try to access "fi": it causes segmentation fault.
    if (DEBUG_PRINT_COMMAND) {
        char* _path = (char*) malloc(mount_dir_len+strlen(path)+4);
        resolve_prefix(path, _path);
        logger(DEBUG, "CHOWN, %s, %d, %d, %p\n",
               _path, uid, gid, fi);
        free(_path);
    }

    int fh;
    int locate_err = locate(path, fh);
    if (locate_err != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    get_inode_from_inum(&block_inode, fh);
    block_inode.perm_uid = uid;
    block_inode.perm_gid = gid;
    clock_gettime(CLOCK_REALTIME, &block_inode.ctime);
    new_inode_block(&block_inode);

    return 0;
}
