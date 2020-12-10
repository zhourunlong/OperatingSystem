#include "dir.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"
#include "errno.h"

#include <cstring>

int o_opendir(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);

    int fh;
    int locate_err = locate(path, fh);
    fi->fh = fh;
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the directory (error #%d).\n", locate_err);
        return locate_err;
    }

    inode block_inode;
    get_inode_from_inum(&block_inode, fh);
    if (block_inode.mode != MODE_DIR) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] %s is not a directory.\n", path);
        return -ENOTDIR;
    }

    return 0;
}

int o_releasedir(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RELEASEDIR, %s, %p\n", resolve_prefix(path), fi);

    fi->fh = 0;

    return 0;
}

int o_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "READDIR, %s, %p, %p, %d, %p, %d\n",
               resolve_prefix(path), buf, &filler, offset, fi, flags);
    
    int opendir_err = o_opendir(path, fi);
    if (opendir_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot read directory.\n");
        return opendir_err;
    }

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags) 0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags) 0);
    
    inode block_inode;
    get_inode_from_inum(&block_inode, fi->fh);
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1)
                continue;
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j) {
                if (block_dir[j].i_number == 0)
                    continue;
                filler(buf, block_dir[j].filename, NULL, 0, (fuse_fill_dir_flags) 0);
            }
        }
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }
    
    return 0;
}

int o_mkdir(const char* path, mode_t mode) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "MKDIR, %s, %d\n", resolve_prefix(path), mode);
    
    mode |= S_IFDIR;
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(dirname), MAX_FILENAME_LEN);
        return -ENAMETOOLONG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        return locate_err;
    }
    
    inode block_inode;
    get_inode_from_inum(&block_inode, par_inum);
    if (block_inode.mode != MODE_DIR) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
        return -ENOTDIR;
    }

    int tmp_inum;
    locate_err = locate(path, tmp_inum);
    if (locate_err == 0) {
        inode tmp_inode;
        get_inode_from_inum(&tmp_inode, tmp_inum);
        if ((tmp_inode.mode == MODE_FILE) && ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Duplicated name: there is a file with the same name.\n");
        if ((tmp_inode.mode == MODE_DIR) && ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Duplicated name: directory already exists.\n");
        return -EEXIST;
    }

    inode dir_inode;
    file_initialize(&dir_inode, MODE_DIR, mode);
    new_inode_block(&dir_inode, dir_inode.i_number);

    bool rec_avail_for_ins = false;
    inode avail_for_ins, tail_inode;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1) {
                if (!rec_avail_for_ins) {
                    rec_avail_for_ins = true;
                    avail_for_ins = block_inode;
                }
                continue;
            }
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir[j].i_number == 0) {
                    block_dir[j].i_number = dir_inode.i_number;
                    memcpy(block_dir[j].filename, dirname, strlen(dirname) * sizeof(char));
                    int new_block_addr = new_data_block(&block_dir, block_inode.i_number, i);
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

    if (rec_avail_for_ins) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i)
            if (avail_for_ins.direct[i] == -1) {
                int new_block_addr = new_data_block(&block_dir, avail_for_ins.i_number, i);
                avail_for_ins.direct[i] = new_block_addr;
                ++avail_for_ins.num_direct;
                new_inode_block(&avail_for_ins, avail_for_ins.i_number);
                return 0;
            }
    }

    if (DEBUG_DIRECTORY) logger(DEBUG, "create new inode");
    inode append_inode;
    file_initialize(&append_inode, MODE_MID_INODE, 0);
    int new_block_addr = new_data_block(&block_dir,append_inode.i_number, 0);
    append_inode.direct[0] = new_block_addr;
    append_inode.num_direct = 1;
    new_inode_block(&append_inode, append_inode.i_number);
    tail_inode.next_indirect = append_inode.i_number;
    new_inode_block(&tail_inode, tail_inode.i_number);
    return 0;
}

int o_rmdir(const char* path) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));

    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);

    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        return locate_err;
    }
    
    inode block_inode;
    get_inode_from_inum(&block_inode, par_inum);
    if (block_inode.mode != MODE_DIR) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
        return -ENOTDIR;
    }

    inode tail_inode;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1)
                continue;
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir[j].i_number && !strcmp(block_dir[j].filename, dirname)) {
                    inode tmp_inode;
                    get_inode_from_inum(&tmp_inode, block_dir[j].i_number);
                    if (tmp_inode.mode != MODE_DIR) {
                        if (ERROR_DIRECTORY)
                            logger(ERROR, "[ERROR] %s is not a directory.\n", path);
                        return -ENOTDIR;
                    }
                    while (1) {
                        for (int l = 0; l < NUM_INODE_DIRECT; ++l) {
                            if (tmp_inode.direct[l] == -1)
                                continue;
                            directory tmp_dir;
                            get_block(&tmp_dir, tmp_inode.direct[l]);
                            for (int s = 0; s < MAX_DIR_ENTRIES; ++s)
                                if (tmp_dir[s].i_number != 0) {
                                    logger(ERROR, "%s is not empty!\n", path);
                                    return -ENOTEMPTY;
                                }
                        }
                        if (tmp_inode.next_indirect == 0)
                            break;
                        get_inode_from_inum(&tmp_inode, tmp_inode.next_indirect);
                    }

                    int cnt = 0;
                    for (int k = 0; k < MAX_DIR_ENTRIES; ++k)
                        cnt += (block_dir[k].i_number != 0);
                    if (cnt == 1) {
                        if (block_inode.num_direct != 1 || block_inode.mode == 2) {
                            --block_inode.num_direct;
                            block_inode.direct[i] = -1;
                            new_inode_block(&block_inode, block_inode.i_number);
                        } else {
                            tail_inode.next_indirect = block_inode.next_indirect;
                            new_inode_block(&tail_inode, tail_inode.i_number);
                        }
                    } else {
                        block_dir[j].i_number = 0;
                        memset(block_dir[j].filename, 0, sizeof(block_dir[j].filename));
                        int new_block_addr = new_data_block(&block_dir, block_inode.i_number, i);
                        block_inode.direct[i] = new_block_addr;
                        new_inode_block(&block_inode, block_inode.i_number);
                    }
                    return 0;
                }
        }
        tail_inode = block_inode;
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }

    if (ERROR_DIRECTORY)
        logger(ERROR, "[ERROR] Directory does not exist.\n");
    return -ENOENT;
}
