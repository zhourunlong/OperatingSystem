#include "metadata.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "index.h"
#include "errno.h"

#include <cstring>

extern struct options options;

int o_getattr(const char* path, struct stat* sbuf, struct fuse_file_info* fi) {
    logger(DEBUG, "GETATTR, %s, %p, %p\n", resolve_prefix(path), sbuf, fi);
    

	/* ****************************************
	 * Resolve path and find file inode structure.
	 * ****************************************/
    int i_number;
    int locate_error = locate(path, i_number);
	if (locate_error != 0) {
		logger(ERROR, "No such file or directory.\n");
		flag = -ENOENT;
	}

    struct inode f_inode;
    get_inode_from_inum((void*)&f_inode, i_number);
    if (f_inode.i_number != i_number) {
        logger(ERROR, "Corrupted global inode table.");
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
			flag = EEXIST;  // File does not exist.
			break;
		default:
			logger(ERROR, "Unknown file type.\n");
			flag = EPERM;  // Unknown file type.
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
    logger(DEBUG, "ACCESS, %s, %d\n", resolve_prefix(path), mode);
    return 0;
}