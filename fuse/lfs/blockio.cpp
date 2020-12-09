#include "blockio.h"

#include "logger.h"
#include "utility.h"

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <cstring>
#include <vector>


/** Retrieve block according to the block address.
 * @param  data: pointer of return data.
 * @param  block_addr: block address.
 * Note that the block may be in segment buffer, or in disk file. */
void get_block(void* data, int block_addr) {
    int segment = block_addr / BLOCKS_IN_SEGMENT;
    int block = block_addr % BLOCKS_IN_SEGMENT;

    if (segment == cur_segment) {    // Data in segment buffer.
        int buffer_offset = block * BLOCK_SIZE;
        memcpy(data, segment_buffer + buffer_offset, BLOCK_SIZE);
    } else {    // Data in disk file.
        read_block(data, block_addr);
    }
}


/** Create a new data block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @param  i_number: i_number of the file (that the data belongs to).
 * @param  direct_index: the index of direct[] in that inode, pointing to the new block.
 * @return block_addr: global block address of the new block.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_data_block(void* data, int i_number, int direct_index) {
    int buffer_offset = cur_block * BLOCK_SIZE;
    int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;

    // Append data block.
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

    // Append segment summary for this block.
    add_segbuf_summary(cur_block, i_number, direct_index);
    
    // Increment cur_block, and flush segment buffer if it is full.
    if (cur_block == BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        write_segment(segment_buffer, cur_segment);
        cur_segment++;
        cur_block = 0;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }
    
    return block_addr;
}


/** Create a new inode block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @param  i_number: i_number of the added inode.
 * @return block_addr: global block address of the new block.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_inode_block(void* data, int i_number) {
    int buffer_offset = cur_block * BLOCK_SIZE;
    int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;

    // Append data block.
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

    // Append segment summary for this block.
    // [CAUTION] We use index -1 to represent an inode, rather than a direct[] pointer.
    add_segbuf_summary(cur_block, i_number, -1);

    // Append imap entry for this inode, and update inode_table.
    add_segbuf_imap(i_number, block_addr);
    inode_table[i_number] = block_addr;
    
    // Increment cur_block, and flush segment buffer if it is full.
    if (cur_block == BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        write_segment(segment_buffer, cur_segment);
        cur_segment++;
        cur_block = 0;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }

    return block_addr;
}


/** Append a segment summary entry for a given block.
 * @param  block_index: index of the new block.
 * @param  i_number: i_number of the file (that the data belongs to).
 * @param  direct_index: the index of direct[] in that inode, pointing to the new block. 
 * [CAUTION] We use index -1 to represent an inode, which is NOT a direct[] pointer. */
void add_segbuf_summary(int block_index, int i_number, int direct_index) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : i_number,
        direct_index : direct_index
    };
    memcpy(segment_buffer + buffer_offset, blk_summary, entry_size);
}


/** Append a imap entry for a given inode block.
 * @param  i_number: i_number of the added inode.
 * @param  block_addr: global block address of the added inode. */
void add_segbuf_imap(int i_number, int block_addr) {
    int entry_size = sizeof(struct imap_entry);
    int buffer_offset = IMAP_OFFSET + next_imap_index * entry_size;
    imap_entry blk_imentry = {
        i_number     : i_number,
        inode_block  : block_addr
    };
    memcpy(segment_buffer + buffer_offset, blk_imentry, entry_size);
    next_imap_index++;
}


/** Initialize a new file by creating its inode.
 * @param  cur_inode: struct for the new inode (should be manually allocated before function call).
 * @param  mode: type of the file (1 = file, 2 = dir; -1 = non-head).
 * @param  permission: using UGO x RWX format in base-8 (e.g., 0777). */
void file_initialize(struct inode* cur_inode, int mode, int permission) {
    count_inode++;

    cur_inode->i_number     = count_inode;
    cur_inode->mode         = mode;
    cur_inode->num_links    = 1;
    cur_inode->fsize_byte   = 0;
    cur_inode->fsize_block  = 1;
    cur_inode->io_block     = 1;
    cur_inode->permission   = permission;
    cur_inode->perm_uid     = USER_UID;
    cur_inode->perm_gid     = USER_GID;
    cur_inode->device       = USER_DEVICE;
    cur_inode->num_direct   = 0;
    memset(cur_inode->direct, -1, sizeof(cur_inode->direct));
    cur_inode->next_indirect = -1;
}


/** Append to the new file by adding new data blocks.
 * @param  cur_inode: struct for the file inode.
 * @param  data: buffer for the file data block to be appended. */
void file_add_data(struct inode* cur_inode, void* data) {
    int block_addr = new_data_block(data, cur_inode->i_number, cur_inode->num_direct);
    cur_inode->direct[cur_inode->num_direct] = block_addr;

    if (cur_inode->num_direct == NUM_INODE_DIRECT) {    // File is too large, so another inode is necessary.
        struct inode next_inode;
        int next_inumber = file_initialize(next_inode);
        cur_inode->next_indirect = &next_inode;         // Here we temporarily use next_indirect field for next_inode address.
    } else {
        cur_inode->num_direct++;
    }
}


/** Commit a new file by storing its inode in log.
 * @param  cur_inode: struct for the file inode. */
void file_commit(struct inode* cur_inode) {
    time_t cur_time;
    time(&cur_time);
    cur_inode->atime = (int)cur_time;
    cur_inode->mtime = (int)cur_time;
    cur_inode->ctime = (int)cur_time;

    // The (possible) chain of inodes should be stored in reverse order.
    std::vector<struct inode*> inode_chain;
    inode_chain.push_back(cur_inode);
    while 
}


/** Generate a checkpoint and save it to disk file. */
void generate_checkpoint() {
    checkpoints ckpt;
    read_checkpoints(&ckpt);

    time_t cur_time;
    time(&cur_time);

    memcpy(ckpt[next_checkpoint].segment_bitmap, segment_bitmap, sizeof(segment_bitmap));
    ckpt[next_checkpoint].count_inode = count_inode;
    ckpt[next_checkpoint].cur_block = cur_block;
    ckpt[next_checkpoint].cur_segment = cur_segment;
    ckpt[next_checkpoint].root_dir_inumber = root_dir_inumber;
    ckpt[next_checkpoint].timestamp = (int) cur_time;

    write_checkpoints(&ckpt);
    next_checkpoint = 1 - next_checkpoint;
}