#include "file.h"

#include "logger.h"
#include "path.h"
#include "dir.h"
#include "index.h"
#include "errno.h"
#include "blockio.h"
#include "utility.h"

#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

extern struct options options;
extern int file_handle;

int o_open(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);
    
    int inode_num;
    int flag;    
    flag = locate(path, inode_num);
    fi -> fh = (uint64_t) inode_num;

    return flag;
}

int o_release(const char* path, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path), fi);

    return 0;
}

int o_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "READ, %s, %p, %d, %d, %p\n",
               resolve_prefix(path), buf, size, offset, fi);
    
    size_t len;
    int inode_num = fi -> fh;
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    len = cur_inode.fsize_byte;
    int t_offset = offset;
    if (cur_inode.mode != 1)
        return 0;
    if (offset < len) {
        if(offset + size > len)
            size = len - offset;
    }
    else return 0;

    // locate the inode
    while (t_offset > 0) {
        if(t_offset < cur_inode.num_direct * BLOCK_SIZE) {
            break;
        }
        t_offset -= cur_inode.num_direct * BLOCK_SIZE;
        get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
    }
    
    int cur_block_offset = t_offset, cur_block_ind;
    // locate the block
    cur_block_ind = t_offset / BLOCK_SIZE;
    cur_block_offset -= cur_block_ind * BLOCK_SIZE;

    int cur_buf_pos = 0;
    int copy_size = BLOCK_SIZE;
    char loader[BLOCK_SIZE + 10];
    while (cur_buf_pos <= size) {
        get_block(loader, cur_inode.direct[cur_block_ind]);
        copy_size = BLOCK_SIZE - cur_block_offset;
        if(size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        memcpy(buf + cur_buf_pos, loader + cur_block_offset, copy_size);
        cur_buf_pos += copy_size;
        cur_block_offset += copy_size;
        if (cur_buf_pos == size)
            break;
        if(cur_block_offset == BLOCK_SIZE) {
            if(cur_block_ind + 1 == cur_inode.num_direct) {
                get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
                cur_block_ind = cur_block_offset = 0;
            }
            else {
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

    size_t len;
    int inode_num = fi -> fh;
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    inode head_inode;
    len = cur_inode.fsize_byte;
    int t_offset = offset;
    
    if (cur_inode.mode != MODE_FILE) {
        return 0;
    }
    if (offset > len) {
        return 0;
    }

    //locate the inode
    
    bool is_END = false;

    while (true) {
        if(t_offset < cur_inode.num_direct * BLOCK_SIZE) {
            break;
        }
        t_offset -= cur_inode.num_direct * BLOCK_SIZE;
        if(t_offset == 0 && cur_inode.next_indirect == 0) {
            is_END = true;
            break;
        }
        get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
    }

    int cur_block_offset = t_offset, cur_block_ind;
    // locate the block
    cur_block_ind = t_offset / BLOCK_SIZE;
    cur_block_offset -= cur_block_ind * BLOCK_SIZE;

    int cur_buf_pos = 0;
    int copy_size = BLOCK_SIZE;
    char loader[BLOCK_SIZE + 10];

    if(is_END == false) {
        while (cur_buf_pos < size && cur_buf_pos + offset < ((int) (len + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE) {
            get_block(loader, cur_inode.direct[cur_block_ind]);
            // print(loader, 3);
            copy_size = BLOCK_SIZE - cur_block_offset;
            if(size - cur_buf_pos < copy_size)
                copy_size = size - cur_buf_pos;
            memcpy(loader + cur_block_offset, buf + cur_buf_pos, copy_size);
            cur_buf_pos += copy_size;
            cur_block_offset += copy_size;
            // printf("%d\n", copy_size);
            file_modify(&cur_inode, cur_block_ind, loader);
            get_block(loader, cur_inode.direct[cur_block_ind]);
            // print(loader, 3);
            if (cur_buf_pos == size) {
                // printf("%s\n", loader);
                break;
            }
            if(cur_block_offset == BLOCK_SIZE && cur_buf_pos + offset != ((len + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE) {
                if(cur_block_ind + 1 == cur_inode.num_direct) {
                    new_inode_block(&cur_inode); //???
                    get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
                    cur_block_ind = cur_block_offset = 0;
                }
                else {
                    cur_block_ind += 1;
                    cur_block_offset = 0;
                }
            }
        }
        new_inode_block(&cur_inode);
        printf("pass 1: %d.\n", cur_inode.i_number);
        if (cur_buf_pos == size) {
            get_inode_from_inum(&head_inode, inode_num);
            if(cur_buf_pos + offset > len) {
                head_inode.fsize_byte = cur_buf_pos + offset;
                // printf("write inode\n");
                // print(&head_inode);
                new_inode_block(&head_inode);
            }
            // print(&head_inode);
            return size;
        }
    }

    memset(loader, 0, sizeof(loader));
    
    len = ((len + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    while (cur_buf_pos < size) {
        copy_size = BLOCK_SIZE;
        if(size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        memset(loader, 0, sizeof(loader));
        memcpy(loader, buf + cur_buf_pos, copy_size);
        file_add_data(&cur_inode, loader);
        cur_buf_pos += copy_size;
        len += copy_size;
    }
    new_inode_block(&cur_inode);
    get_inode_from_inum(&head_inode, inode_num);
    head_inode.fsize_byte = len;
    new_inode_block(&head_inode);

    return cur_buf_pos;
}

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "CREATE, %s, %d, %p\n",
               resolve_prefix(path), mode, fi);
    
    mode |= S_IFREG;
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Directory name too long: length %d > %d.\n", strlen(dirname), MAX_FILENAME_LEN);
        return -E2BIG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the parent directory (error #%d).\n", locate_err);
        return locate_err;
    }
    
    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    if (head_inode.mode != MODE_DIR) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] %s is not a directory.\n", parent_dir);
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
        return -EEXIST;
    }
    
    inode *file_inode = new inode;
    file_initialize(file_inode, MODE_FILE, mode);
    print(file_inode);

    fi -> fh = file_inode -> i_number;
    
    int flag = append_parent_dir_entry(head_inode, dirname, file_inode -> i_number);
    new_inode_block(file_inode);
    return flag;
}

int o_rename(const char* from, const char* to, unsigned int flags) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "RENAME, %s, %s, %d\n",
               resolve_prefix(from), resolve_prefix(to), flags);
    
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    
    char* from_parent_dir = relative_to_absolute(from, "../", 0);
    char* to_parent_dir = relative_to_absolute(to, "../", 0);
    char* from_name = current_fname(from);
    char* to_name = current_fname(to);
    if (strlen(to_name) >= MAX_FILENAME_LEN) {
        logger(ERROR, "dirname too long (%d), should < %d!\n", strlen(to_name), MAX_FILENAME_LEN);
        return -ENAMETOOLONG;
    }
    int from_inum;
    int to_inum;
    int from_par_inum;
    int to_par_inum;
    int locate_err;

    locate_err = locate(from_parent_dir, from_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "error loading parent dir!\n");
        return locate_err;
    }
    locate_err = locate(to_parent_dir, to_par_inum);
    if (locate_err != 0) {
        logger(ERROR, "error loading parent dir!\n");
        return locate_err;
    }
    
    locate_err = locate(from, from_inum);
    if (locate_err != 0) {
        logger(ERROR, "error loading source dir!\n");
        return locate_err;
    }
    locate_err = locate(to, to_inum);
    if (locate_err == 0) {
        logger(ERROR, "already exist!\n");
        return -EEXIST;
    }

    inode from_inode;
    get_inode_from_inum(&from_inode, from_inum);
    
    inode from_par_inode;
    get_inode_from_inum(&from_par_inode, from_par_inum);

    directory block_dir;
    bool find = false;
    while (true) {
        for (int i = 0; i < NUM_INODE_DIRECT; i++) {
            if (from_par_inode.direct[i] != -1) {
                get_block(&block_dir, from_par_inode.direct[i]); //& need to be deleted
                for (int j = 0; j < MAX_DIR_ENTRIES; j++)
                    if(block_dir[j].i_number == from_inum) {
                        find = true;
                        block_dir[j].i_number = 0;
                        break;
                    }
                if(find == true) {
                    file_modify(&from_par_inode, i, block_dir);
                    break;
                }
            }
            if(find == true)
                break;
        }
        if(find == true)
            break;
        new_inode_block(&from_par_inode);
        get_inode_from_inum(&from_par_inode, from_par_inode.next_indirect);
    }
    new_inode_block(&from_par_inode);

    inode head_inode;
    get_inode_from_inum(&head_inode, to_par_inum);
    int flag = append_parent_dir_entry(head_inode, to_name, from_inum);
    return flag;
}

int o_unlink(const char* path) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "UNLINK, %s\n", resolve_prefix(path));
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* file_name = current_fname(path);
    int par_inum, file_inum;
    int locate_err = locate(parent_dir, par_inum);
    locate_err = locate(path, file_inum);
    if (locate_err != 0) {
        if (ERROR_FILE)
            logger(ERROR, "[ERROR] Cannot open the file(error #%d).\n", locate_err);
        return locate_err;
    }
    inode head_inode;
    get_inode_from_inum(&head_inode, par_inum);
    int flag = remove_object(head_inode, file_name, MODE_FILE);
    return flag;
}

int o_link(const char* src, const char* dest) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "LINK, %s, %s\n", resolve_prefix(src), resolve_prefix(dest));
    char* src_parent_dir = relative_to_absolute(src, "../", 0);
    char* dest_parent_dir = relative_to_absolute(dest, "../", 0);
    char* src_name = current_fname(src);
    char* dest_name = current_fname(dest);

    if (strlen(src_name) >= MAX_FILENAME_LEN || strlen(dest_name) >= MAX_FILENAME_LEN) {
        return - ENAMETOOLONG;
    }

    int src_par_inum, dest_par_inum;
    int src_inum, dest_inum;
    inode src_inode;

    int locate_err = locate(src, src_inum);
    if (locate_err != 0) {
        return locate_err;
    }
    get_inode_from_inum(&src_inode, src_inum);
    if (src_inode.mode != MODE_FILE) {
        return -EISDIR;
    }

    locate_err = locate(dest_parent_dir, dest_par_inum);
    if (locate_err != 0) {
        return locate_err;
    }
    locate_err = locate(dest, dest_inum);
    if (locate_err == 0) {
        return -EEXIST;
    }

    inode dest_par_inode;
    get_inode_from_inum(&dest_par_inode, dest_par_inum);
    int flag = append_parent_dir_entry(dest_par_inode, dest_name, src_inum);
    inode src_inode;
    get_inode_from_inum(&src_inode, src_inum);
    src_inode.num_links += 1;
    new_inode_block(&src_inode);
    return flag;
}

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "TRUNCATE, %s, %d, %p\n",
               resolve_prefix(path), size, fi);

    return 0;
}