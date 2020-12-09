#include "file.h"

#include "logger.h"
#include "path.h"
#include "index.h"
#include "errno.h"
#include "blockio.h"
#include "utility.h"

#include <cstring>
#include <errno.h>

extern struct options options;
extern int file_handle;

int o_open(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);
    int inode_num;
    int flag;    
    flag = locate(path, inode_num);
    fi -> fh = (uint64_t) inode_num;
    return flag;
    return 0;
}

int o_release(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path), fi);

    return 0;
}

int o_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
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
    logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);
    size_t len;
    int inode_num = fi -> fh;
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    inode head_inode = cur_inode;
    len = cur_inode.fsize_byte;
    int t_offset = offset;
    if (cur_inode.mode != 1)
        return 0;
    if (offset > len) {
        return 0;
    }
    //locate the inode
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
    while (cur_buf_pos < size && cur_buf_pos + offset < ((len - 1) / BLOCK_SIZE + 1) * BLOCK_SIZE) {
        get_block(loader, cur_inode.direct[cur_block_ind]);
        copy_size = BLOCK_SIZE - cur_block_offset;
        if(size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        memcpy(loader + cur_block_offset, buf + cur_buf_pos, copy_size);
        cur_buf_pos += copy_size;
        cur_block_offset += copy_size;
        file_modify(&cur_inode, cur_block_ind, loader);
        if (cur_buf_pos == size)
            break;
        if(cur_block_offset == BLOCK_SIZE && cur_buf_pos + offset != ((len - 1) / BLOCK_SIZE + 1) * BLOCK_SIZE) {
            if(cur_block_ind + 1 == cur_inode.num_direct) {
                file_commit(&cur_inode);
                get_inode_from_inum(&cur_inode, cur_inode.next_indirect);
                cur_block_ind = cur_block_offset = 0;
            }
            else {
                cur_block_ind += 1;
                cur_block_offset = 0;
            }
        }
    }
    file_commit(&cur_inode);
    if (cur_buf_pos == size) {
        if(cur_buf_pos + offset > len) {
            head_inode.fsize_byte = cur_buf_pos + offset;
            file_commit(&head_inode);
        }
        return size;
    }
    
    len = ((len - 1) / BLOCK_SIZE + 1) * BLOCK_SIZE;
    while (cur_buf_pos < size) {
        copy_size = BLOCK_SIZE;
        if(size - cur_buf_pos < copy_size)
            copy_size = size - cur_buf_pos;
        memcpy(loader, buf + cur_buf_pos, copy_size);
        file_add_data(&cur_inode, loader);
        file_commit(&cur_inode);
        cur_buf_pos += copy_size;
        len += copy_size;
    }
    head_inode.fsize_byte = len;
    file_commit(&head_inode);
}

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CREATE, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
    mode |= S_IFREG;
    char* parent_dir = relative_to_absolute(path, "../", 0);
    char* dirname = current_fname(path);
    if (strlen(dirname) >= MAX_FILENAME_LEN) {
        logger(ERROR, "dirname too long (%d), should < %d!\n", strlen(dirname), MAX_FILENAME_LEN);
        return -E2BIG;
    }
    int par_inum;
    int locate_err = locate(parent_dir, par_inum);
    if (locate_err != 0) {
        logger(ERROR, "error loading parent dir!\n");
        return locate_err;
    }
    
    inode block_inode;
    get_inode_from_inum(&block_inode, par_inum);
    if (block_inode.mode != MODE_DIR) {
        logger(ERROR, "%s is not a directory!\n", parent_dir);
        return -ENOTDIR;
    }

    int tmp_inum;
    locate_err = locate(path, tmp_inum);
    if (locate_err == 0) {
        inode tmp_inode;
        get_inode_from_inum(&tmp_inode, tmp_inum);
        if (tmp_inode.mode == MODE_FILE)
            logger(ERROR, "there is a file with same name!\n");
        if (tmp_inode.mode == MODE_DIR)
            logger(ERROR, "directory already exists!\n");
        return -EEXIST;
    }

    inode file_inode;
    file_initialize(&file_inode, MODE_DIR, mode);
    file_commit(&file_inode);
    
    int avail_direct_idx = 0;
    bool rec_avail_for_ins = false;
    inode avail_for_ins, tail_inode;
    while (1) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i) {
            if (block_inode.direct[i] == -1) {
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
                    block_dir[j].i_number = file_inode.i_number;
                    memcpy(block_dir[j].filename, dirname, strlen(dirname) * sizeof(char));
                    int new_block_addr = new_data_block(&block_dir, block_inode.i_number, i);
                    block_inode.direct[i] = new_block_addr;
                    ++block_inode.num_direct;
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
    block_dir[0].i_number = file_inode.i_number;
    memcpy(block_dir[0].filename, dirname, strlen(dirname) * sizeof(char));

    directory block_dir;
    memset(block_dir, 0, sizeof(block_dir));
    block_dir[0].i_number = file_inode.i_number;
    memcpy(block_dir[0].filename, dirname, strlen(dirname) * sizeof(char));

    if (rec_avail_for_ins) {
        for (int i = 0; i < NUM_INODE_DIRECT; ++i)
            if (avail_for_ins.direct[i] == -1) {
                int new_block_addr = new_data_block(&block_dir, avail_for_ins.i_number, avail_direct_idx);
                avail_for_ins.direct[i] = new_block_addr;
                ++avail_for_ins.num_direct;
                new_inode_block(&avail_for_ins, avail_for_ins.i_number);
                return 0;
            }
    }

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

int o_rename(const char* from, const char* to, unsigned int flags) {
    logger(DEBUG, "RENAME, %s, %s, %d\n",
        resolve_prefix(from), resolve_prefix(to), flags);
    return 0;
}

int o_unlink(const char* path) {
    logger(DEBUG, "UNLINK, %s\n", resolve_prefix(path));
    return 0;
}

int o_link(const char* src, const char* dest) {
    logger(DEBUG, "LINK, %s, %s\n", resolve_prefix(src), resolve_prefix(dest));
    return 0;
}

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
    logger(DEBUG, "TRUNCATE, %s, %d, %p\n",
        resolve_prefix(path), size, fi);
    return 0;
}