#include "metadata.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "index.h"

#include <cstring>

extern struct options options;

int o_getattr(const char* path, struct stat* sbuf, struct fuse_file_info* fi) {
    logger(DEBUG, "GETATTR, %s, %p, %p\n", resolve_prefix(path), sbuf, fi);
    
    // Resolve path and find file inode structure.
    int i_number;
    locate(path, i_number);

    struct inode f_inode;
    get_block((void*)&f_inode, inode_table[i_number]);

    if (f_inode.i_number != i_number) {
        logger(ERROR, "Corrupted global inode table.");
        exit(-1);
    }

    sbuf->st_ino = i_number;                /* File serial number. */
    sbuf->st_mode;                          /* File mode. */
    sbuf->st_nlink = f_inode.num_links;     /* Link count. */
    sbuf->st_uid = f_inode.perm_uid;        /* User ID of the file's owner. */
    sbuf->st_gid = f_inode.perm_gid;        /* Group ID of the file's group. */
    sbuf->st_dev = f_inode.device;          /* Device. */
    sbuf->st_rdev = f_inode.device;         /* Device number, if device. */
    sbuf->st_size;                          /* Size of file, in bytes. */
    sbuf->st_blksize;                       /* Optimal block size for I/O. */
    sbuf->st_blocks;                        /* Number 512-byte blocks allocated. */
    sbuf->st_atim;                          /* Time of last access. */
    sbuf->st_mtim;                          /* Time of last modification.  */
    sbuf->st_ctim;                          /* Time of last status change.  */

    (void) fi;
    int flag = 0;

    memset(sbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        sbuf->st_mode = S_IFDIR | 0755;
        sbuf->st_nlink = 2;
    } else if (strcmp(path+1, options.filename) == 0) {
        sbuf->st_mode = S_IFREG | 0444;
        sbuf->st_nlink = 1;
        sbuf->st_size = strlen(options.contents);
    } else
        flag = -ENOENT;

    return res;
}


int o_access(const char* path, int mode) {
    logger(DEBUG, "ACCESS, %s, %d\n", resolve_prefix(path), mode);
    return 0;
}