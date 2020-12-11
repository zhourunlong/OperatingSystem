#include "dir.h"

#include "logger.h"
#include "path.h"
#include "utility.h"
#include "blockio.h"
#include "errno.h"

#include <cstring>
#include <stdlib.h>
#include <time.h>

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
    
    inode block_inode, head_inode;
    get_inode_from_inum(&head_inode, fi->fh);
    block_inode = head_inode;
    bool accessed = false;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1)
                continue;
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            accessed = true;
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
    
    if (accessed && FUNC_ATIME_DIR) {
        struct timespec cur_time;
        clock_gettime(CLOCK_REALTIME, &cur_time);
        update_atime(head_inode, cur_time);
        new_inode_block(&head_inode, head_inode.i_number);
    }

    return 0;
}

/** Append an entry for a new file / directory in its parent directory.
 * @param  head_inode: first i_node of the parent directory.
 * @param  new_name: name of the new file / directory.
 * @param  new_inum: i_number of the new file / directory.
 * @return flag: 0 on success, standard negative error codes on error. */
int append_parent_dir_entry(struct inode &head_inode, const char* new_name, int new_inum) {
    inode block_inode = head_inode;
    
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    bool rec_avail_for_ins = false, accessed = false, firblk = true, afi_firblk, tail_firblk;
    inode avail_for_ins, tail_inode;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1) {
                if (!rec_avail_for_ins) {
                    rec_avail_for_ins = true;
                    avail_for_ins = block_inode;
                    afi_firblk = firblk;
                }
                continue;
            }
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            accessed = true;
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir[j].i_number == 0) {
                    block_dir[j].i_number = new_inum;
                    memcpy(block_dir[j].filename, new_name, strlen(new_name) * sizeof(char));
                    int new_block_addr = new_data_block(&block_dir, block_inode.i_number, i);
                    block_inode.direct[i] = new_block_addr;
                    
                    if (firblk) {
                        if (FUNC_ATIME_DIR)
                            update_atime(block_inode, cur_time);
                        block_inode.mtime = block_inode.ctime = cur_time;
                    } else {
                        if (FUNC_ATIME_DIR)
                            update_atime(head_inode, cur_time);
                        head_inode.mtime = head_inode.ctime = cur_time;
                        new_inode_block(&head_inode, head_inode.i_number);
                    }
                    new_inode_block(&block_inode, block_inode.i_number);
                    return 0;
                }
        }
        tail_inode = block_inode;
        tail_firblk = firblk;
        firblk = false;
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }

    directory block_dir;
    memset(block_dir, 0, sizeof(block_dir));
    block_dir[0].i_number = new_inum;
    memcpy(block_dir[0].filename, new_name, strlen(new_name) * sizeof(char));

    if (rec_avail_for_ins) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i)
            if (avail_for_ins.direct[i] == -1) {
                int new_block_addr = new_data_block(&block_dir, avail_for_ins.i_number, i);
                avail_for_ins.direct[i] = new_block_addr;
                ++avail_for_ins.num_direct;
                
                if (afi_firblk) {
                    if (FUNC_ATIME_DIR)
                        update_atime(avail_for_ins, cur_time);
                    avail_for_ins.mtime = avail_for_ins.ctime = cur_time;
                } else {
                    if (FUNC_ATIME_DIR)
                        update_atime(head_inode, cur_time);
                    head_inode.mtime = head_inode.ctime = cur_time;
                    new_inode_block(&head_inode, head_inode.i_number);
                }
                new_inode_block(&avail_for_ins, avail_for_ins.i_number);
                return 0;
            }
    }

    if (DEBUG_DIRECTORY)
        logger(DEBUG, "create new inode");
    inode append_inode;
    file_initialize(&append_inode, MODE_MID_INODE, 0);
    int new_block_addr = new_data_block(&block_dir, append_inode.i_number, 0);
    append_inode.direct[0] = new_block_addr;
    append_inode.num_direct = 1;
    new_inode_block(&append_inode, append_inode.i_number);
    tail_inode.next_indirect = append_inode.i_number;
    if (tail_firblk)
        tail_inode.ctime = cur_time;
    else {
        head_inode.ctime = cur_time;
        new_inode_block(&head_inode, head_inode.i_number);
    }
    new_inode_block(&tail_inode, tail_inode.i_number);
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
    
    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    if (head_inode.mode != MODE_DIR) {
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
            logger(ERROR, "[ERROR] Duplicated name: there exists a file with the same name.\n");
        if ((tmp_inode.mode == MODE_DIR) && ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Duplicated name: there exists a directory with the same name.\n");
        return -EEXIST;
    }

    inode dir_inode;
    file_initialize(&dir_inode, MODE_DIR, mode);
    new_inode_block(&dir_inode, dir_inode.i_number);

    int flag = append_parent_dir_entry(head_inode, dirname, dir_inode.i_number);
    return flag;
}

/** Remove a file / directory from LFS, and the corresponding entry in its parent directory.
 * @param  head_inode: first i_node of the parent directory.
 * @param  del_name: name of the new file / directory.
 * @param  del_mode: whether to delete a file-link (MODE_FILE) or a directory (MODE_DIR).
 * @return flag: 0 on success, standard negative error codes on error. */
int remove_object(struct inode &head_inode, const char* del_name, int del_mode) {
    inode block_inode = head_inode;

    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    bool accessed = false, firblk = true, tail_firblk;
    inode tail_inode;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1)
                continue;
            directory block_dir;
            get_block(&block_dir, block_inode.direct[i]);
            accessed = true;
            for (int j = 0; j < MAX_DIR_ENTRIES; ++j)
                if (block_dir[j].i_number && !strcmp(block_dir[j].filename, del_name)) {
                    inode tmp_inode, tmp_head_inode;
                    get_inode_from_inum(&tmp_head_inode, block_dir[j].i_number);

                    // Verify del_name refers to an expected type of object.
                    if ((del_mode == MODE_DIR) && (tmp_head_inode.mode != MODE_DIR)) {
                        if (ERROR_DIRECTORY)
                            logger(ERROR, "[ERROR] %s is not a directory.\n", del_name);
                        return -ENOTDIR;
                    } else if ((del_mode == MODE_FILE) && (tmp_head_inode.mode != MODE_FILE)) {
                        if (ERROR_FILE)
                            logger(ERROR, "[ERROR] %s is not a file.\n", del_name);
                        return -ENOTDIR;  // WRONG: TO BE ADDED!!!!
                    }

                    // Ensure empty directories: "rmdir" only works for empty directories.
                    if (del_mode == MODE_DIR) {
                        tmp_inode = tmp_head_inode;
                        while (1) {
                            for (int l = 0; l < NUM_INODE_DIRECT; ++l) {
                                if (tmp_inode.direct[l] == -1)
                                    continue;
                                directory tmp_dir;
                                get_block(&tmp_dir, tmp_inode.direct[l]);
                                for (int s = 0; s < MAX_DIR_ENTRIES; ++s)
                                    if (tmp_dir[s].i_number != 0) {
                                        if (ERROR_DIRECTORY)
                                            logger(ERROR, "[ERROR] Directory %s is not empty.\n", del_name);
                                        if (FUNC_ATIME_DIR) {
                                            update_atime(tmp_head_inode, cur_time);
                                            new_inode_block(&tmp_head_inode, tmp_head_inode.i_number);
                                        }
                                        return -ENOTEMPTY;
                                    }
                            }
                            if (tmp_inode.next_indirect == 0)
                                break;
                            get_inode_from_inum(&tmp_inode, tmp_inode.next_indirect);
                        }
                    }

                    // Remove parent directory entry: discard empty inodes.
                    int cnt = 0;
                    for (int k = 0; k < MAX_DIR_ENTRIES; ++k)
                        cnt += (block_dir[k].i_number != 0);
                    if (cnt == 1) {
                        if (block_inode.num_direct != 1 || block_inode.mode == MODE_DIR) {
                            --block_inode.num_direct;
                            block_inode.direct[i] = -1;
                            if (firblk) {
                                if (FUNC_ATIME_DIR)
                                    update_atime(block_inode, cur_time);
                                block_inode.mtime = block_inode.ctime = cur_time;
                            } else {
                                if (FUNC_ATIME_DIR)
                                    update_atime(head_inode, cur_time);
                                head_inode.mtime = head_inode.ctime = cur_time;
                                new_inode_block(&head_inode, head_inode.i_number);
                            }
                            new_inode_block(&block_inode, block_inode.i_number);
                        } else {
                            tail_inode.next_indirect = block_inode.next_indirect;
                            if (tail_firblk) {
                                if (FUNC_ATIME_DIR)
                                    update_atime(tail_inode, cur_time);
                                tail_inode.mtime = tail_inode.ctime = cur_time;
                            } else {
                                if (FUNC_ATIME_DIR)
                                    update_atime(head_inode, cur_time);
                                head_inode.mtime = head_inode.ctime = cur_time;
                                new_inode_block(&head_inode, head_inode.i_number);
                            }
                            new_inode_block(&tail_inode, tail_inode.i_number);
                        }
                    } else {
                        block_dir[j].i_number = 0;
                        memset(block_dir[j].filename, 0, sizeof(block_dir[j].filename));
                        int new_block_addr = new_data_block(&block_dir, block_inode.i_number, i);
                        block_inode.direct[i] = new_block_addr;
                        if (firblk) {
                            if (FUNC_ATIME_DIR)
                                update_atime(block_inode, cur_time);
                            block_inode.mtime = block_inode.ctime = cur_time;
                        } else {
                            if (FUNC_ATIME_DIR)
                                update_atime(head_inode, cur_time);
                            head_inode.mtime = head_inode.ctime = cur_time;
                            new_inode_block(&head_inode, head_inode.i_number);
                        }
                        new_inode_block(&block_inode, block_inode.i_number);
                    }

                    // Remove i_map and i_table pointers to the object.
                    if (del_mode == MODE_DIR) {
                        inode_table[block_dir[j].i_number] = -1;

                        // To ensure the number of imap entries not larger than blocks,
                        // we have to create an empty "inode" block for deleted dir.
                        // We have to use "new_data_block" to manually add imap entry.
                        add_segbuf_imap(block_dir[j].i_number, -1);
                        char* buf = (char*) malloc(BLOCK_SIZE);
                        memset(buf, 0, sizeof(buf));
                        new_data_block(buf, block_dir[j].i_number, -1);
                        free(buf);
                    } else if (del_mode == MODE_FILE) {
                        inode file_inode;
                        get_inode_from_inum(&file_inode, block_dir[j].i_number);

                        if (file_inode.num_links == 1) {
                            // Delete the file (the same as dir).
                            inode_table[block_dir[j].i_number] = -1;

                            add_segbuf_imap(block_dir[j].i_number, -1);
                            char* buf = (char*) malloc(BLOCK_SIZE);
                            memset(buf, 0, sizeof(buf));
                            new_data_block(buf, block_dir[j].i_number, -1);
                            free(buf);
                        } else {
                            // Decrement link count by 1, and update ctime.
                            file_inode.num_links--;
                            file_inode.ctime = cur_time;
                            new_inode_block(&file_inode, file_inode.i_number);
                        }
                    }

                    return 0;
                }
        }
        tail_inode = block_inode;
        tail_firblk = firblk;
        firblk = false;
        if (block_inode.next_indirect == 0)
            break;
        get_inode_from_inum(&block_inode, block_inode.next_indirect);
    }

    if ((del_mode == MODE_DIR) && (ERROR_DIRECTORY)) {
        logger(ERROR, "[ERROR] Directory does not exist.\n"); 
    } else if ((del_mode == MODE_FILE) && (ERROR_FILE)) {
        logger(ERROR, "[ERROR] File does not exist.\n"); 
    }
    if (accessed && FUNC_ATIME_DIR) {
        update_atime(head_inode, cur_time);
        new_inode_block(&head_inode, head_inode.i_number);
    }
    return -ENOENT;
}

int o_rmdir(const char* path) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));

    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);

    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        return locate_err;
    }

    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    if (head_inode.mode != MODE_DIR) {
        if (ERROR_DIRECTORY)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
        return -ENOTDIR;
    }

    int flag = remove_object(head_inode, dirname, MODE_DIR);
    return flag;
}
