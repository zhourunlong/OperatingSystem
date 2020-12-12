#include "file.h"

#include "logger.h"
#include "print.h"
#include "path.h"
#include "dir.h"
#include "index.h"
#include "blockio.h"
#include "utility.h"

#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


/** (for internal uses only) Set direct[block_ind:] in cur_inode to -1.
 * @param  cur_inode: the inode to be operated on.
 * @param  block_ind: the start position of truncation.
 * [CAUTION] Since num_direct = block_ind + 1, we allow block_ind to be -1. */
void truncate_inode(inode &cur_inode, int block_ind) {
    cur_inode.num_direct = block_ind + 1;
    cur_inode.next_indirect = 0;
    for (int i = cur_inode.num_direct; i < NUM_INODE_DIRECT; i++) {
        cur_inode.direct[i] = -1;
    }
}


/** (for internal uses only) Write the specified segment of file.
 * @param  ...: please refer to standard ".write" interface. */
int write_in_file(const char* path, const char* buf, size_t size,
                  off_t offset, struct fuse_file_info* fi) {
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    if (cur_inode.mode != MODE_FILE) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", path);
        return 0;
    }
    
    // Write permission control (read permission managed by get_inode_from_inum).
    int perm_flag = 0;
    struct fuse_context* user_info = fuse_get_context();
    if (ENABLE_PERMISSION && (((user_info->uid == cur_inode.perm_uid) && !(cur_inode.permission & 0200))
                          || ((user_info->gid == cur_inode.perm_gid) && !(cur_inode.permission & 0020))
                          || !(cur_inode.permission & 0002)) )
        { perm_flag = -EACCES; }
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return 0;
    }

    len = cur_inode.fsize_byte;
    int t_offset = offset;

    // Locate the first inode.
    bool is_end = false;
    while (true) {
        if (t_offset < cur_inode.num_direct * BLOCK_SIZE) {
            break;
        }
        t_offset -= cur_inode.num_direct * BLOCK_SIZE;
        if (t_offset == 0 && cur_inode.next_indirect == 0) {
            is_end = true;
            break;
        }
        get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
    }

    // Locate the first block.
    int cur_block_offset = t_offset, cur_block_ind;
    cur_block_ind = t_offset / BLOCK_SIZE;
    cur_block_offset -= cur_block_ind * BLOCK_SIZE;

    // Write data retrieved from buffer.
    int cur_buf_pos = 0;
    int copy_size = BLOCK_SIZE;
    char loader[BLOCK_SIZE + 10];
    inode head_inode;
    if (is_end == false) {
        int cur_file_blksize = ((int) (len+BLOCK_SIZE-1) / BLOCK_SIZE) * BLOCK_SIZE;
        while (cur_buf_pos < size && cur_buf_pos + offset < cur_file_blksize) {
            get_block(loader, cur_inode.direct[cur_block_ind]);

            // Copy a block: copy_size = min(BLOCK_SIZE-cur_block_offset, size-cur_buf_pos).
            copy_size = BLOCK_SIZE - cur_block_offset;
            if (size - cur_buf_pos < copy_size)
                copy_size = size - cur_buf_pos;
            memcpy(loader + cur_block_offset, buf + cur_buf_pos, copy_size);
            file_modify(&cur_inode, cur_block_ind, loader);
            get_block(loader, cur_inode.direct[cur_block_ind]);

            // Update buffer pointer and block pointer.
            cur_buf_pos += copy_size;
            cur_block_offset += copy_size;
            if (cur_buf_pos == size)
                break;
            if (cur_block_offset == BLOCK_SIZE && cur_buf_pos + offset != cur_file_blksize) {
                if (cur_block_ind + 1 == cur_inode.num_direct) {  // Reach the end of an inode.
                    if (cur_inode.num_direct != NUM_INODE_DIRECT) {
                        logger(ERROR, "[FATAL ERROR] Corrupt file system: inode leak.\n");
                        exit(-1);
                    }
                    new_inode_block(&cur_inode);
                    get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
                    cur_block_ind = cur_block_offset = 0;
                } else {
                    cur_block_ind += 1;
                    cur_block_offset = 0;
                }
            }
        }
        
        // Finish because reaching the end of writing: update timestamps.
        if (cur_buf_pos == size) {
            if (cur_buf_pos + offset > len) {
                if (cur_inode.i_number == inode_num) {
                    cur_inode.fsize_byte = cur_buf_pos + offset;
                    cur_inode.fsize_block = (cur_buf_pos + offset + BLOCK_SIZE - 1) / BLOCK_SIZE;
                    update_atime(cur_inode, cur_time);
                    cur_inode.mtime = cur_time;
                    new_inode_block(&cur_inode);
                } else {
                    new_inode_block(&cur_inode);
                    get_inode_from_inum(&head_inode, inode_num);
                    head_inode.fsize_byte = cur_buf_pos + offset;
                    head_inode.fsize_block = (cur_buf_pos + offset + BLOCK_SIZE - 1) / BLOCK_SIZE;
                    update_atime(head_inode, cur_time);
                    head_inode.mtime = cur_time;
                    new_inode_block(&head_inode);
                }
            } else {
                if (cur_inode.i_number == inode_num) {
                    update_atime(cur_inode, cur_time);
                    cur_inode.mtime = cur_time;
                    new_inode_block(&cur_inode);
                } else {
                    new_inode_block(&cur_inode);
                    get_inode_from_inum(&head_inode, inode_num);
                    update_atime(head_inode, cur_time);
                    head_inode.mtime = cur_time;
                    new_inode_block(&head_inode);
                }
            }
            return size;
        }
        new_inode_block(&cur_inode);
    }
    
    // Start creating non-existing blocks.
    len = ((len + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    while (cur_buf_pos < size) {
        // Copy a block: copy_size = min(BLOCK_SIZE, size-cur_buf_pos).
        copy_size = BLOCK_SIZE;
        if (size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        
        memset(loader, 0, sizeof(loader));
        memcpy(loader, buf + cur_buf_pos, copy_size);
        file_add_data(&cur_inode, loader);

        cur_buf_pos += copy_size;
        len += copy_size;
    }

    // Update timestamps.
    new_inode_block(&cur_inode);
    get_inode_from_inum(&head_inode, inode_num);
    head_inode.fsize_byte = len;
    head_inode.fsize_block = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    update_atime(head_inode, cur_time);
    head_inode.mtime = cur_time;
    new_inode_block(&head_inode);

    return cur_buf_pos;
}


int o_open(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);

    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    int inode_num;
    int flag = locate(path, inode_num);
    if (flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the file (error #%d).\n", flag);
        return flag;
    }
    fi -> fh = (uint64_t) inode_num;

    int flags = fi -> flags;
    if ((flags & O_TRUNC) && (flags & O_ACCMODE)) {
        inode cur_inode;
        get_inode_from_inum(&cur_inode, inode_num);
        truncate_inode(cur_inode, -1);
        cur_inode.fsize_block = cur_inode.fsize_byte = 0;
        cur_inode.ctime = cur_time;
        new_inode_block(&cur_inode);
    }

    return 0;
}

int o_release(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path), fi);

    fi = nullptr;

    return 0;
}

int o_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "READ, %s, %p, %d, %d, %p\n",
               resolve_prefix(path), buf, size, offset, fi);
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    // In case the file is not open yet.
    size_t len;
    if (fi == nullptr) {
        int first_flag = 0;
        first_flag = o_open(path, fi);
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return 0;
        }
    }
    int inode_num = fi -> fh;
    
    inode cur_inode;
    int perm_flag = get_inode_from_inum(&cur_inode, inode_num);
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        return 0;
    }

    // Update access time.
    update_atime(cur_inode, cur_time);
    new_inode_block(&cur_inode);
    
    len = cur_inode.fsize_byte;
    int t_offset = offset;
    if (cur_inode.mode != MODE_FILE) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", path);
        return 0;
    }
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
    } else {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot read from an offset larger than file length.\n", path);
        return 0;
    }

    // Locate the start inode.
    while (t_offset > 0) {
        if (t_offset < cur_inode.num_direct * BLOCK_SIZE) {
            break;
        }
        t_offset -= cur_inode.num_direct * BLOCK_SIZE;
        get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
    }
    
    // Locate the start block.
    int cur_block_offset = t_offset, cur_block_ind;
    cur_block_ind = t_offset / BLOCK_SIZE;
    cur_block_offset -= cur_block_ind * BLOCK_SIZE;

    // Copy data to buffer.
    int cur_buf_pos = 0;
    int copy_size = BLOCK_SIZE;
    char loader[BLOCK_SIZE + 10];
    while (cur_buf_pos < size) {
        get_block(loader, cur_inode.direct[cur_block_ind]);

        // Copy a block: copy_size = min(BLOCK_SIZE-cur_block_offset, size-cur_buf_pos).
        copy_size = BLOCK_SIZE - cur_block_offset;
        if (size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        memcpy(buf + cur_buf_pos, loader + cur_block_offset, copy_size);
        
        // Update buffer pointer and block pointer.
        cur_buf_pos += copy_size;
        cur_block_offset += copy_size;
        if (cur_buf_pos == size)
            break;
        if (cur_block_offset == BLOCK_SIZE) {
            if (cur_block_ind + 1 == cur_inode.num_direct) {
                get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
                cur_block_ind = cur_block_offset = 0;
            } else {
                cur_block_ind += 1;
                cur_block_offset = 0;
            }
        }
    }

    return size;
}



int o_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
               resolve_prefix(path), buf, size, offset, fi);

    // In case the file is not open yet.
    size_t len;
    if (fi == nullptr) {
        int first_flag = 0;
        first_flag = o_open(path, fi);
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return 0;
        }
    }
    int inode_num = fi -> fh;

    inode cur_inode;
    int perm_flag = 0;
    get_inode_from_inum(&cur_inode, inode_num);
    
    // Write permission control (read permission managed by get_inode_from_inum).
    struct fuse_context* user_info = fuse_get_context();
    if (ENABLE_PERMISSION && (((user_info->uid == cur_inode.perm_uid) && !(cur_inode.permission & 0200))
                          || ((user_info->gid == cur_inode.perm_gid) && !(cur_inode.permission & 0020))
                          || !(cur_inode.permission & 0002)) )
        { perm_flag = -EACCES; }
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return 0;
    }

    // If offset exceeds current length, pad 0 in the gap.
    len = cur_inode.fsize_byte;
    if (offset > len) {
        char padding_buf[offset - len + 1];
        memset(padding_buf, 0, sizeof(padding_buf));
        write_in_file(path, padding_buf, offset - len, len, fi);
    }

    int write_len = write_in_file(path, buf, size, offset, fi);
    return write_len;
}

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CREATE, %s, %o, %p\n",
               resolve_prefix(path), mode, fi);
    
    mode &= 0777;
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(dirname), MAX_FILENAME_LEN);
        return -ENAMETOOLONG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        return locate_err;
    }

    inode head_inode;
    int perm_flag;
    perm_flag = get_inode_from_inum(&head_inode, par_inum);
    if (perm_flag != 0)
        return perm_flag;
    if (head_inode.mode != MODE_DIR) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
        return -ENOTDIR;
    }
    
    int tmp_inum;
    locate_err = locate(path, tmp_inum);
    if (locate_err == 0) {
        inode tmp_inode;
        perm_flag = get_inode_from_inum(&tmp_inode, tmp_inum);
        if (perm_flag != 0)
            return perm_flag;
        if ((tmp_inode.mode == MODE_FILE) && ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: there is a file with the same name.\n");
        if ((tmp_inode.mode == MODE_DIR) && ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: directory already exists.\n");
        return -EEXIST;
    }
    
    inode file_inode;
    file_initialize(&file_inode, MODE_FILE, mode);
    fi -> fh = file_inode.i_number;
    
    int flag = append_parent_dir_entry(head_inode, dirname, file_inode.i_number);
    new_inode_block(&file_inode);
    return flag;
}

int o_rename(const char* from, const char* to, unsigned int flags) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RENAME, %s, %s, %d\n",
               resolve_prefix(from), resolve_prefix(to), flags);
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    char* from_parent_dir = relative_to_absolute(from, "../", 0);
    char* to_parent_dir = relative_to_absolute(to, "../", 0);
    char* from_name = current_fname(from);
    char* to_name = current_fname(to);
    int from_inum, to_inum, from_par_inum, to_par_inum;
    if (strlen(to_name) >= MAX_FILENAME_LEN) {
        logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(to_name), MAX_FILENAME_LEN);
        return -ENAMETOOLONG;
    }

    int locate_err;
    locate_err = locate(from_parent_dir, from_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Fail to locate source parent directory.\n");
        return locate_err;
    }
    locate_err = locate(to_parent_dir, to_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Fail to locate destination parent directory.\n");
        return locate_err;
    }
    locate_err = locate(from, from_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Source file does not exist.\n");
        return locate_err;
    }

    locate_err = locate(to, to_inum);  // This is not necessarily an error.
    if (locate_err == 0) {  // Destination file already exists.
        if (flags == RENAME_NOREPLACE) {
            logger(ERROR, "[ERROR] Destination file already exists, and cannot be overwritten.\n");
            return -EEXIST;
        }

        int perm_flag;
        inode from_inode;
        perm_flag = get_inode_from_inum(&from_inode, from_inum);
        perm_flag = 0;
        if (perm_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Permission denied: not allowed to read source file inode.\n");
            return perm_flag;
        }
        inode from_par_inode;
        perm_flag = get_inode_from_inum(&from_par_inode, from_par_inum);
        perm_flag = 0;
        struct fuse_context* user_info = fuse_get_context();
        if (ENABLE_PERMISSION && (((user_info->uid == from_par_inode.perm_uid) && !(from_par_inode.permission & 0600))
            || ((user_info->gid == from_par_inode.perm_gid) && !(from_par_inode.permission & 0060))
            || !(from_par_inode.permission & 0006)) )
        { perm_flag = -EACCES; }
        if (perm_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Permission denied: not allowed to write source dir inode.\n");
            return perm_flag;
        }
        inode to_inode;
        perm_flag = get_inode_from_inum(&to_inode, to_inum);
        perm_flag = 0;
        if (perm_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Permission denied: not allowed to read dest file inode.\n");
            return perm_flag;
        }
        inode to_par_inode;
        perm_flag = get_inode_from_inum(&to_par_inode, to_par_inum);
        perm_flag = 0;
        if (ENABLE_PERMISSION && (((user_info->uid == to_par_inode.perm_uid) && !(to_par_inode.permission & 0600))
            || ((user_info->gid == to_par_inode.perm_gid) && !(to_par_inode.permission & 0060))
            || !(to_par_inode.permission & 0006)) )
        { perm_flag = -EACCES; }
        if (perm_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Permission denied: not allowed to write dest dir inode.\n");
            return perm_flag;
        }
        
        // Update timestamps.
        to_par_inode.mtime = cur_time;
        from_par_inode.mtime = cur_time;
        to_par_inode.ctime = cur_time;
        from_par_inode.ctime = cur_time;
        new_inode_block(&to_par_inode);
        new_inode_block(&from_par_inode);
        
        // Remove destination directory entry.
        remove_parent_dir_entry(to_par_inode, to_inum);

        // Conflict case 1: exchange files.
        if (flags == RENAME_EXCHANGE) {
            // On exchange, remove source directory entry as well.
            remove_parent_dir_entry(from_par_inode, from_inum);

            // Update timestamps.
            to_inode.ctime = cur_time;
            new_inode_block(&to_inode);

            inode head_inode;
            get_inode_from_inum(&head_inode, to_par_inum);
            append_parent_dir_entry(head_inode, from_name, from_inum);

            get_inode_from_inum(&head_inode, from_par_inum);
            int flag = append_parent_dir_entry(head_inode, to_name, to_inum);
            return flag;
        }
    } 
    
    // Conflict case 2: overwrite destination file.
    // Base case: destination file does not exist.
    inode from_inode;
    int perm_flag = get_inode_from_inum(&from_inode, from_inum);
    perm_flag = 0;
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read source file inode.\n");
        return perm_flag;
    }
    inode from_par_inode;
    perm_flag = get_inode_from_inum(&from_par_inode, from_par_inum);
    perm_flag = 0;
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read source dir inode.\n");
        return perm_flag;
    }
    
    // Update timestamps.
    from_par_inode.mtime = cur_time;
    from_inode.ctime = cur_time;
    new_inode_block(&from_par_inode);
    new_inode_block(&from_inode);

    // Remove source directory entry.
    remove_parent_dir_entry(from_par_inode, from_inum);

    inode head_inode;
    perm_flag = get_inode_from_inum(&head_inode, to_par_inum);
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read dest file inode.\n");
        return perm_flag;
    }
    int flag = append_parent_dir_entry(head_inode, to_name, from_inum);
    return flag;
}

int o_unlink(const char* path) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UNLINK, %s\n", resolve_prefix(path));
    
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* file_name = current_fname(path);
    
    int par_inum, file_inum;
    locate(parent_dir, par_inum);
    int locate_err = locate(path, file_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the file (error #%d).\n", locate_err);
        return locate_err;
    }

    inode head_inode;
    int perm_flag = get_inode_from_inum(&head_inode, par_inum);
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        return perm_flag;
    }
    int flag = remove_object(head_inode, file_name, MODE_FILE);
    return flag;
}

int o_link(const char* src, const char* dest) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "LINK, %s, %s\n", resolve_prefix(src), resolve_prefix(dest));
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    char* src_parent_dir = relative_to_absolute(src, "../", 0);
    char* dest_parent_dir = relative_to_absolute(dest, "../", 0);
    char* src_name = current_fname(src);
    char* dest_name = current_fname(dest);

    if (strlen(src_name) >= MAX_FILENAME_LEN || strlen(dest_name) >= MAX_FILENAME_LEN) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Name too long: length %d or %d > %d.\n", strlen(src_name), strlen(dest_name), MAX_FILENAME_LEN);
        return -ENAMETOOLONG;
    }

    int src_par_inum, dest_par_inum;
    int src_inum, dest_inum;
    inode src_inode;

    int locate_err = locate(src, src_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open source file (error #%d).\n", locate_err);
        return locate_err;
    }
    int perm_flag = get_inode_from_inum(&src_inode, src_inum);
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read source file.\n");
        return perm_flag;
    }
    if (src_inode.mode != MODE_FILE) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", src_name);
        return -EISDIR;
    }

    locate_err = locate(dest_parent_dir, dest_par_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the destination directory (error #%d).\n", locate_err);
        return locate_err;
    }
    locate_err = locate(dest, dest_inum);
    if (locate_err == 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: there exists a file / directory with the same name.\n");
        return -EEXIST;
    }

    inode dest_par_inode;
    perm_flag = get_inode_from_inum(&dest_par_inode, dest_par_inum);
    perm_flag = 0;
    struct fuse_context* user_info = fuse_get_context();
    if (ENABLE_PERMISSION && (((user_info->uid == dest_par_inode.perm_uid) && !(dest_par_inode.permission & 0600))
        || ((user_info->gid == dest_par_inode.perm_gid) && !(dest_par_inode.permission & 0060))
        || !(dest_par_inode.permission & 0006)) )
    { perm_flag = -EACCES; }
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write dest directory.\n");
        return perm_flag;
    }
    int flag = append_parent_dir_entry(dest_par_inode, dest_name, src_inum);

    src_inode.num_links += 1;
    src_inode.ctime = cur_time;
    new_inode_block(&src_inode);
    return flag;
}

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "TRUNCATE, %s, %d, %p\n",
               resolve_prefix(path), size, fi);
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    // In case the file is not open yet.
    if (fi == nullptr) {
        int first_flag = 0;
        first_flag = o_open(path, fi);
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return first_flag;
        }
    }
    int inode_num = fi -> fh;

    inode cur_inode;
    int perm_flag = get_inode_from_inum(&cur_inode, inode_num);
    struct fuse_context* user_info = fuse_get_context();
    if (ENABLE_PERMISSION && (((user_info->uid == cur_inode.perm_uid) && !(cur_inode.permission & 0200))
        || ((user_info->gid == cur_inode.perm_gid) && !(cur_inode.permission & 0020))
        || !(cur_inode.permission & 0002)) )
    { perm_flag = -EACCES; }
    if (perm_flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return perm_flag;
    }
    if (cur_inode.mode == MODE_DIR) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", path);
        return -EISDIR;
    }
    
    int len = cur_inode.fsize_byte;
    if (size >= len)    // Do not need to truncate.
        return 0;
    
    cur_inode.fsize_byte = size;
    cur_inode.fsize_block = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    update_atime(cur_inode, cur_time);
    cur_inode.mtime = cur_time;
    new_inode_block(&cur_inode);
    off_t t_offset = size;
    while (t_offset > 0) {
        if (t_offset < cur_inode.num_direct * BLOCK_SIZE) {
            break;
        }
        t_offset -= cur_inode.num_direct * BLOCK_SIZE;
        get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
    }
    int cur_block_offset = t_offset, cur_block_ind;
    cur_block_ind = t_offset / BLOCK_SIZE;
    cur_block_offset -= cur_block_ind * BLOCK_SIZE;
    truncate_inode(cur_inode, cur_block_ind);
    new_inode_block(&cur_inode);
    return 0;
}