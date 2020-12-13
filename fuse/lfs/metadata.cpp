#include "metadata.h"

#include "logger.h"
#include "print.h"
#include "path.h"
#include "utility.h"
#include "index.h"
#include "blockio.h"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fuse.h>

extern struct options options;

int o_getattr(const char* path, struct stat* sbuf, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "GETATTR, %s, %p, %p\n", resolve_prefix(path).c_str(), sbuf, fi);
    

    /* ****************************************
     * Resolve path and find file inode structure.
     * ****************************************/
    int i_number;
    int locate_error = locate(path, i_number);
    if (locate_error != 0) {
        if (ERROR_METADATA)
            logger(ERROR, "[ERROR] Cannot access the path (error #%d).\n", locate_error);
        return locate_error;
    }

    // Since getattr() is very fundamental, we shall always allow attribute reading.
    // Otherwise, commands like chmod, chown, etc. do not work correctly.
    struct inode f_inode;
    get_inode_from_inum((void*)&f_inode, i_number);
    if (DEBUG_METADATA_INODE) print(&f_inode);

    if (f_inode.i_number != i_number) {
        printf("inum = %d\n", i_number);
        //print_inode_table();
        logger(ERROR, "[FATAL ERROR] Corrupt file system on disk: inode inconsistent with inumber.\n");
        exit(-1);
    }


    /* ****************************************
     * Fill all "struct stat" fields.
     * ****************************************/
    int flag = 0;
    memset(sbuf, 0, sizeof(struct stat));

    // Basic information.
    sbuf->st_ino = i_number;                /* File serial number. */
    sbuf->st_nlink = f_inode.num_links;     /* Link count. */

    // File mode (see stat.h).
    switch (f_inode.mode) {
        case (MODE_FILE):
            sbuf->st_mode = S_IFREG | f_inode.permission;
            break;
        case (MODE_DIR):
            sbuf->st_mode = S_IFDIR | f_inode.permission;
            break;
        case (MODE_MID_INODE):
            logger(ERROR, "[ERROR] Unexpected access to non-head inode #%d.\n", i_number);
            flag = -EEXIST;  // File does not exist.
            break;
        default:
            logger(ERROR, "[ERROR] Unknown file type in inode #%d.\n", i_number);
            flag = -EPERM;  // Unknown file type.
    }
    sbuf->st_uid = f_inode.perm_uid;        /* User ID of the file's owner. */
    sbuf->st_gid = f_inode.perm_gid;        /* Group ID of the file's group. */

    // File size.
    sbuf->st_size = f_inode.fsize_byte;     /* Size of file, in bytes. */
    sbuf->st_blocks = f_inode.fsize_block;  /* Number of blocks allocated. */
    sbuf->st_blksize = f_inode.io_block;    /* Optimal block size for I/O. */

    // Device information.
    sbuf->st_dev = f_inode.device;          /* Device. */
    sbuf->st_rdev = 0;                      /* Device number, if file is a device. */

    // Time stamps
    sbuf->st_atim = f_inode.atime;          /* Time of last access. */
    sbuf->st_mtim = f_inode.mtime;          /* Time of last modification.  */
    sbuf->st_ctim = f_inode.ctime;          /* Time of last status change.  */

    return flag;
}


int o_access(const char* path, int mode) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "ACCESS, %s, %d\n", resolve_prefix(path).c_str(), mode);

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();

    /* Mode 0 (F_OK): test whether file exists (by default). */
    int i_number;
    int locate_error = locate(path, i_number);
    if (locate_error != 0) {
        if (ERROR_METADATA)
            logger(ERROR, "[Error] Cannot access the path (error #%d).\n", locate_error);
        return locate_error;
    }
    
    /* Mode 1~7 (in base-8): test file permissions; may be ORed toghether. */ 
    struct inode f_inode;
    int perm_flag = get_inode_from_inum((void*)&f_inode, i_number);
    if (perm_flag != 0) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        return perm_flag;
    }
    if (DEBUG_METADATA_INODE) print(&f_inode);
    
    if (f_inode.i_number != i_number) {
        printf("inum = %d\n", i_number);
        //print_inode_table();
        logger(ERROR, "[FATAL ERROR] Corrupt file system on disk: inode inconsistent with inumber.\n");
        exit(-1);
    }
    
    // Mode 4 (R_OK): test read permission.
    if (verify_permission(PERM_READ, &f_inode, user_info, (mode & R_OK) && ENABLE_ACCESS_PERM))
        return -EACCES;
    
    // Mode 2 (W_OK): test write permission.
    if (verify_permission(PERM_WRITE, &f_inode, user_info, (mode & W_OK) && ENABLE_ACCESS_PERM))
        return -EACCES;
    
    // Mode 1 (X_OK): test write permission.
    if (verify_permission(PERM_EXEC, &f_inode, user_info, (mode & X_OK) && ENABLE_ACCESS_PERM))
        return -EACCES;


    /* If it survives till here, the requested permission is granted. */
    return 0;
}