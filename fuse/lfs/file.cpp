#include "file.h"

#include "logger.h"
#include "path.h"
#include "index.h"
#include "blockio.h"
#include "utility.h"

#include <cstring>
#include <errno.h>

extern struct options options;
extern int file_handle;

int o_open(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);
    
    if (strcmp(path+1, options.filename) != 0)
            return -ENOENT;

    if ((fi->flags & O_ACCMODE) == O_WRONLY)
            return -EACCES;

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
    (void) fi;
    if(strcmp(path+1, options.filename) != 0)
        return -ENOENT;
    int inode_num = fi -> fh;
    inode cur_inode;
    get_inode_from_inum(&cur_inode, inode_num);
    len = cur_inode.fsize_byte;
    int t_offset = offset;
    if (cur_inode.mode != 1)
        return -EISDIR;
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
        copy_size = BLOCK_SIZE;
        if(size - cur_buf_pos < BLOCK_SIZE)
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
            }
        }
    }
    return size;
}

int o_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);
    return 0;
}

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CREATE, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
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