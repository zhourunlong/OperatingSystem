#include "file.h"

#include "logger.h"
#include "print.h"
#include "path.h"
#include "dir.h"
#include "index.h"
#include "blockio.h"
#include "utility.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

const int SC = sizeof(char);

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
    
    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();
    
    int inode_num = fi->fh;
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    if (cur_inode.mode != MODE_FILE) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", path);
        return 0;
    }
    
    // Write permission control.
    if (!verify_permission(PERM_WRITE, &cur_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return 0;
    }

    size_t len = cur_inode.fsize_byte;
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
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path).c_str(), fi);

    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    int inode_num;
    int flag = locate(path, inode_num);
    if (flag != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the file (error #%d).\n", flag);
        return flag;
    }

    // Retrieve open flags, the inode and current user info.
    int flags = fi -> flags;
    struct fuse_context* user_info = fuse_get_context();
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);

    // Verify file permissions.
    int open_perm = flags & O_ACCMODE;
    if (((open_perm == O_RDONLY) || (open_perm == O_RDWR)) && (!verify_permission(PERM_READ, &cur_inode, user_info, ENABLE_PERMISSION))) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        return -EACCES;
    }
    if (((open_perm == O_WRONLY) || (open_perm == O_RDWR)) && (!verify_permission(PERM_WRITE, &cur_inode, user_info, ENABLE_PERMISSION))) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return -EACCES;
    }
    
    // Return file handle if the user has due permission.
    fi->fh = (uint64_t) inode_num;

    // Handle O_TRUNC flag.
    if ((flags & O_TRUNC) && (flags & O_ACCMODE)) {
        if (is_full) {
            logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
            logger(WARN, "====> Cannot proceed to truncate the file: flag O_TRUNC is omitted.\n");
            return 0;
        }

        truncate_inode(cur_inode, -1);
        cur_inode.fsize_block = cur_inode.fsize_byte = 0;
        cur_inode.ctime = cur_time;
        new_inode_block(&cur_inode);
    }

    return 0;
}

int o_release(const char* path, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path).c_str(), fi);

    fi->fh = 0;

    return 0;
}

int o_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "READ, %s, %p, %d, %d, %p\n",
               resolve_prefix(path).c_str(), buf, size, offset, fi);

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();

    // In case the file is not open yet.
    size_t len;
    if (fi->fh == 0) {
        int first_flag = 0, fh = 0;
        first_flag = locate(path, fh);
        fi->fh = fh;
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return 0;
        }
    }
    int inode_num = fi->fh;
    
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    if (!verify_permission(PERM_READ, &cur_inode,user_info ,ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        return 0;
    }
    
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

    // Update access time.
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to update timestamps, but the file is still accessible.\n");
        return 0;
    } else {
        timespec cur_time;
        clock_gettime(CLOCK_REALTIME, &cur_time);

        update_atime(cur_inode, cur_time);
        new_inode_block(&cur_inode);
    }

    return size;
}

int o_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
               resolve_prefix(path).c_str(), buf, size, offset, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to write into the file.\n");
        return -ENOSPC;
    }

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();

    // In case the file is not open yet.
    size_t len;
    if (fi->fh == 0) {
        int first_flag = 0, fh = 0;
        first_flag = locate(path, fh);
        fi->fh = fh;
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return 0;
        }
    }
    int inode_num = fi->fh;

    inode cur_inode;
    int perm_flag = 0;
    get_inode_from_inum(&cur_inode, inode_num);
    if (!verify_permission(PERM_WRITE, &cur_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
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
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CREATE, %s, %o, %p\n",
               resolve_prefix(path).c_str(), mode, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to create a new file.\n");
        return -ENOSPC;
    }
    
    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();

    mode &= 0777;
    
    std::string tmp = relative_to_absolute(path, "../", 0);
    char* parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(parent_dir, tmp.c_str());

    tmp = current_fname(path);
    char* dirname = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(dirname, tmp.c_str());

    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(dirname), MAX_FILENAME_LEN);
        free(parent_dir); free(dirname);
        return -ENAMETOOLONG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        free(parent_dir); free(dirname);
        return locate_err;
    }

    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    if (!verify_permission(PERM_WRITE, &head_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to read.\n");
        free(parent_dir); free(dirname);
        return -EACCES;
    }

    if (head_inode.mode != MODE_DIR) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
        free(parent_dir); free(dirname);
        return -ENOTDIR;
    }
    
    int tmp_inum;
    locate_err = locate(path, tmp_inum);
    if (locate_err == 0) {
        inode tmp_inode;
        get_inode_from_inum(&tmp_inode, tmp_inum);
        if ((tmp_inode.mode == MODE_FILE) && ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: there is a file with the same name.\n");
        if ((tmp_inode.mode == MODE_DIR) && ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: directory already exists.\n");
        free(parent_dir); free(dirname);
        return -EEXIST;
    }
    
    inode file_inode;
    file_initialize(&file_inode, MODE_FILE, mode);
    fi->fh = file_inode.i_number;
    
    int flag = append_parent_dir_entry(head_inode, dirname, file_inode.i_number);
    new_inode_block(&file_inode);
    free(parent_dir); free(dirname);
    return flag;
}

int o_rename(const char* from, const char* to, unsigned int flags) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RENAME, %s, %s, %d\n",
               resolve_prefix(from).c_str(), resolve_prefix(to).c_str(), flags);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to rename the file.\n");
        return -ENOSPC;
    }

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    std::string tmp = relative_to_absolute(from, "../", 0);
    char* from_parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(from_parent_dir, tmp.c_str());

    tmp = relative_to_absolute(to, "../", 0);
    char* to_parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(to_parent_dir, tmp.c_str());

    tmp = current_fname(from);
    char* from_name = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(from_name, tmp.c_str());

    tmp = current_fname(to);
    char* to_name = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(to_name, tmp.c_str());

    int from_inum, to_inum, from_par_inum, to_par_inum;
    if (strlen(to_name) >= MAX_FILENAME_LEN) {
        logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(to_name), MAX_FILENAME_LEN);
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return -ENAMETOOLONG;
    }

    int locate_err;
    locate_err = locate(from_parent_dir, from_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Fail to locate source parent directory.\n");
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return locate_err;
    }
    locate_err = locate(to_parent_dir, to_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Fail to locate destination parent directory.\n");
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return locate_err;
    }
    locate_err = locate(from, from_inum);
    if (locate_err != 0) {
        logger(ERROR, "[ERROR] Source file does not exist.\n");
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return locate_err;
    }

    locate_err = locate(to, to_inum);  // This is not necessarily an error.
    if (locate_err == 0) {  // Destination file already exists.
        if (flags == RENAME_NOREPLACE) {
            logger(ERROR, "[ERROR] Destination file already exists, and cannot be overwritten.\n");
            free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
            return -EEXIST;
        }

        inode from_inode;
        get_inode_from_inum(&from_inode, from_inum);

        inode from_par_inode;
        get_inode_from_inum(&from_par_inode, from_par_inum);
        if (!verify_permission(PERM_WRITE | PERM_READ, &from_par_inode, user_info, ENABLE_PERMISSION)) {
            if (ERROR_PERM)
                logger(ERROR, "[ERROR] Permission denied: not allowed to write source dir inode.\n");
            free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
            return -EACCES;
        }

        inode to_inode;
        get_inode_from_inum(&to_inode, to_inum);

        inode to_par_inode;
        get_inode_from_inum(&to_par_inode, to_par_inum);
        if (!verify_permission(PERM_WRITE | PERM_READ, &to_par_inode, user_info, ENABLE_PERMISSION)) {
            if (ERROR_PERM)
                logger(ERROR, "[ERROR] Permission denied: not allowed to write dest dir inode.\n");
            free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
            return -EACCES;
        }
        
        // Update timestamps.
        to_par_inode.mtime = cur_time;
        from_par_inode.mtime = cur_time;
        to_par_inode.ctime = cur_time;
        from_par_inode.ctime = cur_time;
        new_inode_block(&to_par_inode);
        new_inode_block(&from_par_inode);
        
        // Remove destination directory entry.
        remove_parent_dir_entry(to_par_inode, to_inum, to_name);

        // Conflict case 1: exchange files.
        if (flags == RENAME_EXCHANGE) {
            // On exchange, remove source directory entry as well.
            remove_parent_dir_entry(from_par_inode, from_inum, from_name);

            // Update timestamps.
            to_inode.ctime = cur_time;
            new_inode_block(&to_inode);

            inode head_inode;
            get_inode_from_inum(&head_inode, to_par_inum);
            append_parent_dir_entry(head_inode, from_name, from_inum);

            get_inode_from_inum(&head_inode, from_par_inum);
            int flag = append_parent_dir_entry(head_inode, to_name, to_inum);
            free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
            return flag;
        }
    } 
    
    // Conflict case 2: overwrite destination file.
    // Base case: destination file does not exist.
    inode from_inode;
    get_inode_from_inum(&from_inode, from_inum);
    inode from_par_inode;
    get_inode_from_inum(&from_par_inode, from_par_inum);
    if (!verify_permission(PERM_WRITE | PERM_READ, &from_par_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write source dir inode.\n");
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return -EACCES;
    }
    
    // Update timestamps.
    from_par_inode.mtime = cur_time;
    from_inode.ctime = cur_time;
    new_inode_block(&from_par_inode);
    new_inode_block(&from_inode);

    // Remove source directory entry.
    remove_parent_dir_entry(from_par_inode, from_inum, from_name);

    inode head_inode;
    get_inode_from_inum(&head_inode, to_par_inum);
    if (!verify_permission(PERM_WRITE | PERM_READ, &head_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to dest dir inode.\n");
        free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
        return -EACCES;
    }
    int flag = append_parent_dir_entry(head_inode, to_name, from_inum);
    free(from_parent_dir); free(to_parent_dir); free(from_name); free(to_name);
    return flag;
}

int o_unlink(const char* path) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UNLINK, %s\n", resolve_prefix(path).c_str());
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to unlink the file.\n");
        return -ENOSPC;
    }

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();
    
    std::string tmp = relative_to_absolute(path, "../", 0);
    char* parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(parent_dir, tmp.c_str());

    tmp = current_fname(path);
    char* file_name = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(file_name, tmp.c_str());

    int par_inum, file_inum;
    locate(parent_dir, par_inum);
    int locate_err = locate(path, file_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the file (error #%d).\n", locate_err);
        free(parent_dir); free(file_name);
        return locate_err;
    }

    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    if (!verify_permission(PERM_WRITE | PERM_READ, &head_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to access parent directory.\n");
        free(parent_dir); free(file_name);
        return -EACCES;
    }

    int flag = remove_object(head_inode, file_name, MODE_FILE);
    free(parent_dir); free(file_name);
    return flag;
}

int o_link(const char* src, const char* dest) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "LINK, %s, %s\n", resolve_prefix(src).c_str(), resolve_prefix(dest).c_str());
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to create a hard link.\n");
        return -ENOSPC;
    }

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    std::string tmp = relative_to_absolute(src, "../", 0);
    char* src_parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(src_parent_dir, tmp.c_str());

    tmp = relative_to_absolute(dest, "../", 0);
    char* dest_parent_dir = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(dest_parent_dir, tmp.c_str());

    tmp = current_fname(src);
    char* src_name = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(src_name, tmp.c_str());

    tmp = current_fname(dest);
    char* dest_name = (char*) malloc((tmp.length() + 1) * SC);
    strcpy(dest_name, tmp.c_str());

    if (strlen(src_name) >= MAX_FILENAME_LEN || strlen(dest_name) >= MAX_FILENAME_LEN) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Name too long: length %d or %d > %d.\n", strlen(src_name), strlen(dest_name), MAX_FILENAME_LEN);
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return -ENAMETOOLONG;
    }

    int src_par_inum, dest_par_inum;
    int src_inum, dest_inum;
    inode src_inode;

    int locate_err = locate(src, src_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open source file (error #%d).\n", locate_err);
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return locate_err;
    }
    get_inode_from_inum(&src_inode, src_inum);
    if (src_inode.mode != MODE_FILE) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a file.\n", src_name);
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return -EISDIR;
    }

    locate_err = locate(dest_parent_dir, dest_par_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the destination directory (error #%d).\n", locate_err);
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return locate_err;
    }
    locate_err = locate(dest, dest_inum);
    if (locate_err == 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Duplicated name: there exists a file / directory with the same name.\n");
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return -EEXIST;
    }

    inode dest_par_inode;
    get_inode_from_inum(&dest_par_inode, dest_par_inum);
    if (!verify_permission(PERM_WRITE | PERM_READ, &dest_par_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write dest directory.\n");
        free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
        return -EACCES;
    }
    int flag = append_parent_dir_entry(dest_par_inode, dest_name, src_inum);

    src_inode.num_links += 1;
    src_inode.ctime = cur_time;
    new_inode_block(&src_inode);
    free(src_parent_dir); free(dest_parent_dir); free(src_name); free(dest_name);
    return flag;
}

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
std::lock_guard <std::mutex> guard(global_lock);
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "TRUNCATE, %s, %d, %p\n",
               resolve_prefix(path).c_str(), size, fi);
    
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
        logger(WARN, "====> Cannot proceed to truncate the file.\n");
        return -ENOSPC;
    }

    /* Get information (uid, gid) of the user who calls LFS interface. */
    struct fuse_context* user_info = fuse_get_context();
    
    timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    // In case the file is not open yet.
    if (fi->fh == 0) {
        int first_flag = 0, fh = 0;
        first_flag = locate(path, fh);
        fi->fh = fh;
        if (first_flag != 0) {
            if (ERROR_FILE)
                logger(ERROR, "[ERROR] Cannot open the file. \n");
            return first_flag;
        }
    }
    int inode_num = fi->fh;

    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    if (!verify_permission(PERM_WRITE, &cur_inode, user_info, ENABLE_PERMISSION)) {
        if (ERROR_PERM)
            logger(ERROR, "[ERROR] Permission denied: not allowed to write.\n");
        return -EACCES;
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