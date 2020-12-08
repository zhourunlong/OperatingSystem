#include "dir.h"
#include "logger.h"
#include "prefix.h"
#include "utility.h"

int o_opendir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);

    int locate_err = locate(path, fi->fh);
    if (locate_err != 0)
        return locate_err;

    return 0;
}

int o_releasedir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "RELEASEDIR, %s, %p\n", resolve_prefix(path), fi);

    fi->fh = 0;

    return 0;
}

int o_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    logger(DEBUG, "READDIR, %s, %p, %p, %d, %p, %d\n",
        resolve_prefix(path), buf, &filler, offset, fi, flags);
    
    int opendir_err = o_opendir(path, fi);
    if (opendir_err != 0)
        return opendir_err;

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    
    inode block_inode;
    read_block(block_inode, fi->fh);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == 0)
                continue;
            directory block_dir;
            read_block(block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j) {
                if (block_dir.dir_entry[j].i_number == 0)
                    continue;
                filler(buf, block_dir.dir_entry[j].filename, NULL, 0, (fuse_fill_dir_flags)0);
            }
        }
        if (block_inode.next_indirect == 0)
            break;
        read_block(block_inode, block_inode.next_indirect);
    }
    
    return 0;
}

int o_mkdir(const char* path, mode_t mode) {
    logger(DEBUG, "MKDIR, %s, %d\n", resolve_prefix(path), mode);
    
    mode |= S_IFDIR;
    char* parent_dir = relative_to_absolute(path, "../");
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0)
        return locate_err;
    
    int 
    inode block_inode;
    read_block(block_inode, par_inum);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == 0)
                continue;
            directory block_dir;
            read_block(block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir.dir_entry[j].i_number == 0) {
                    
                }
        }
        if (block_inode.next_indirect == 0)
            break;
        read_block(block_inode, block_inode.next_indirect);
    }

    return 0;
}

int o_rmdir(const char* path) {
    logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));
    return 0;
}
