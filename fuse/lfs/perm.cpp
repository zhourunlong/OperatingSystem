#include "perm.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"

int o_chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CHMOD, %s, %d, %p\n",
               resolve_prefix(path), mode, fi);
    
    int fh;
    int locate_err = locate(path, fh);
    fi->fh = fh;
    if (locate_err != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[Error] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    get_inode_from_inum(&block_inode, fh);
    block_inode.permission = mode;
    new_inode_block(&block_inode, block_inode.i_number);

    return 0;
}

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CHOWN, %s, %d, %d, %p\n",
               resolve_prefix(path), uid, gid, fi);
    
    int fh;
    int locate_err = locate(path, fh);
    fi->fh = fh;
    if (locate_err != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[Error] Cannot access the path (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    get_inode_from_inum(&block_inode, fh);
    block_inode.perm_uid = uid;
    block_inode.perm_gid = gid;
    new_inode_block(&block_inode, block_inode.i_number);

    return 0;
}
