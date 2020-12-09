#include "dir.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"

int o_opendir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);

    int locate_err = locate(path, fi->fh);
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

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    
    inode block_inode;
    read_block_from_inum(block_inode, fi->fh);
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
        read_block_from_inum(block_inode, block_inode.next_indirect);
    }
    
    return 0;
}

int o_mkdir(const char* path, mode_t mode) {
    logger(DEBUG, "MKDIR, %s, %d\n", resolve_prefix(path), mode);
    
    mode |= S_IFDIR;
    char* parent_dir = relative_to_absolute(path, "../"), dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        logger(ERROR, "dirname too long (%d), should < %d!\n", strlen(dirname), MAX_FILENAME_LEN);
        return ???; // return some error code echofs/readdir.cpp
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0)
        return locate_err;
    
    inode dir_inode = new_inode(2, mode); // need initialize i_num, mode, perm

    int par_inum;
    bool rec_afi = false;
    inode block_inode, avail_for_ins, tail_inode;
    read_block_from_inum(block_inode, par_inum);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == 0) {
                if (!rec_afi) {
                    rec_afi = true;
                    avail_for_ins = block_inode;
                }
                continue;
            }
            directory block_dir;
            read_block(block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir.dir_entry[j].i_number == 0) {
                    block_dir.dir_entry[j].i_number = dir_inode.i_number;
                    memcpy(block_dir.dir_entry[j].filename, dirname, strlen(dirname) * sizeof(char));
                    int new_block_addr;
                    int new_block_err = new_block(block_dir, new_block_addr);
                        // new_block need return cur_block, instead of using global var, concurrency issue
                    if (new_block_err != 0) {
                        logger(ERROR, "error when newing dir block!\n");
                        return new_block_err;
                    }
                    block_inode.direct[i] = new_block_addr;
                    // need function move_inode_to_lfs(block_inode)
                    int new_inode_blockaddr;
                    int new_inode_err = new_block(block_inode, new_inode_blockaddr);
                    modify_imap(block_inode.i_number, new_inode_blockaddr);
                    return 0;
                }
        }
        if (block_inode.next_indirect == 0)
            break;
        tail_inode = block_inode;
        read_block_from_inum(block_inode, block_inode.next_indirect);
    }
    
    directory block_dir;
    memset(block_dir, 0, sizeof(block_dir));
    block_dir.dir_entry[0].i_number = dir_inode.i_number;
    memcpy(block_dir.dir_entry[0].filename, dirname, strlen(dirname) * sizeof(char));
    int new_block_addr;
    int new_block_err = new_block(block_dir, new_block_addr);
        // new_block need return cur_block, instead of using global var, concurrency issue
    if (new_block_err != 0) {
        logger(ERROR, "error when newing dir block!\n");
        return new_block_err;
    }

    if (rec_afi) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i)
            if (avail_for_ins.direct[i] == 0) {
                avail_for_ins.direct[i] = new_block_addr;
                int new_inode_blockaddr;
                int new_inode_err = new_block(avail_for_ins, new_inode_blockaddr);
                modify_imap(avail_for_ins.i_number, new_inode_blockaddr);
                return 0;
            }
    }

    inode append_inode = new_inode(-1, 0); // new non-head inode, perm unnecessary
    append_inode.direct[0] = new_block_addr;
    move_inode_to_lfs(append_inode);
    tail_inode.indirect = append_inode.i_number;
    move_inode_to_lfs(tail_inode);

    return 0;
}

int o_rmdir(const char* path) {
    logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));
    return 0;
}
