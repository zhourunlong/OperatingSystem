#include "dir.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"
#include "errno.h"

#include <cstring>

int o_opendir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);

    int fh;
    int locate_err = locate(path, fh);
    fi->fh = fh;
    if (locate_err != 0) {
        logger(ERROR, "error when opening dir!\n");
        return locate_err;
    }

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
    if (opendir_err != 0) {
        logger(ERROR, "error when reading dir!\n");
        return opendir_err;
    }

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags) 0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags) 0);
    
    inode block_inode;
    get_inode_from_inum(&block_inode, fi->fh);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == 0)
                continue;
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j) {
                if (block_dir[j].i_number == 0)
                    continue;
                filler(buf, block_dir[j].filename, NULL, 0, (fuse_fill_dir_flags) 0);
                // sort?
            }
        }
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }
    
    return 0;
}

int o_mkdir(const char* path, mode_t mode) {
    logger(DEBUG, "MKDIR, %s, %d\n", resolve_prefix(path), mode);
    
    mode |= S_IFDIR;
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        logger(ERROR, "dirname too long (%d), should < %d!\n", strlen(dirname), MAX_FILENAME_LEN);
        return -E2BIG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0)
        return locate_err;
    
    inode dir_inode;
    file_initialize(&dir_inode, 2, mode);

    int avail_direct_idx = 0;
    bool rec_avail_for_ins = false;
    inode block_inode, avail_for_ins, tail_inode;
    get_inode_from_inum(&block_inode, par_inum);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == 0) {
                if (!rec_avail_for_ins) {
                    rec_avail_for_ins = true;
                    avail_for_ins = block_inode;
                    avail_direct_idx = i;
                }
                continue;
            }
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir[j].i_number == 0) {
                    block_dir[j].i_number = dir_inode.i_number;
                    memcpy(block_dir[j].filename, dirname, strlen(dirname) * sizeof(char));
                    int new_block_addr = new_data_block(&block_dir, par_inum, i);
                    block_inode.direct[i] = new_block_addr;
                    new_inode_block(&block_inode, block_inode.i_number);
                    return 0;
                }
        }
        tail_inode = block_inode;
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }
    
    directory block_dir;
    memset(block_dir, 0, sizeof(block_dir));
    block_dir[0].i_number = dir_inode.i_number;
    memcpy(block_dir[0].filename, dirname, strlen(dirname) * sizeof(char));
    int new_block_addr = new_data_block(&block_dir, par_inum, avail_direct_idx);

    if (rec_avail_for_ins) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i)
            if (avail_for_ins.direct[i] == 0) {
                avail_for_ins.direct[i] = new_block_addr;
                new_inode_block(&avail_for_ins, avail_for_ins.i_number);
                return 0;
            }
    }

    inode append_inode;
    file_initialize(&append_inode, -1, 0);
    append_inode.direct[0] = new_block_addr;
    new_inode_block(&append_inode, append_inode.i_number);
    tail_inode.next_indirect = append_inode.i_number;
    new_inode_block(&tail_inode, tail_inode.i_number);

    return 0;
}

int o_rmdir(const char* path) {
    logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));
    return 0;
}
