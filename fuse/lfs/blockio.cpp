#include "blockio.h"

#include "logger.h"
#include "utility.h"

#include <unistd.h>
#include <stdio.h>
#include <time.h>


/** Retrieve block according to the block address.
 * @param  data: pointer of return data.
 * @param  block_addr: block address.
 * @return flag: indicating whether the retrieval is successful.
 * Note that the block may be in segment buffer, or in disk file. */
int get_block(void* data, int block_addr) {
    int segment = block_addr / BLOCKS_IN_SEGMENT;
    int block = block_addr % BLOCKS_IN_SEGMENT;

    if (segment == cur_segment) {    // Data in segment buffer.
        int buffer_offset = block * BLOCK_SIZE;
        memcpy(data, segment_buffer + buffer_offset, BLOCKSIZE);
    } else {    // Data in disk file.
        read_block(data, block_addr);
    }
    return 0;
}


/** Create a new data block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @return flag: indicating whether appending is successful.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_data_block(void* data, int i_number, int direct_index) {
    int buffer_offset = cur_block * BLOCK_SIZE;

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
    
    return 0;
}


/** Create a new inode block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @param  i_number: i_number of the added inode.
 * @return flag: indicating whether appending is successful.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_inode_block(void* data, int i_number) {
    int buffer_offset = cur_block * BLOCK_SIZE;

    // Append data block.
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

    // Append segment summary for this block.
    // [CAUTION] We use index -1 to represent an inode, rather than a direct[] pointer.
    add_segbuf_summary(cur_block, i_number, -1);

    // Append imap entry for this inode, and update inode_table.
    int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;
    add_imap_entry(i_number, block_addr);
    inode_table[i_number] = block_addr;
    
    // Increment cur_block, and flush segment buffer if it is full.
    if (cur_block == BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        write_segment(segment_buffer, cur_segment);
        cur_segment++;
        cur_block = 0;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }
    return 0;
}


/** Append a segment summary entry for a given block.
 * @param  block_index: index of the new block.
 * @param  i_number: i_number of the added  block.
 * @param  direct_index: the index of direct[] in that inode, pointing to the new block.
 * @return flag: indicating whether appending is successful. */
int add_segbuf_summary(int block_index, int i_number, int direct_index) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : i_number,
        direct_index : direct_index
    };
    memcpy(segment_buffer + buffer_offset, blk_summary, entry_size);
    return 0;
}


/** Append a imap entry for a given inode block.
 * @param  i_number: i_number of the added inode.
 * @param  block: the index of direct[] in that inode, pointing to the new block.
 * @return flag: indicating whether appending is successful. */
int add_segbuf_summary(int i_number, int block_addr) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : i_number,
        direct_index : direct_index
    };
    memcpy(segment_buffer + buffer_offset, blk_summary, entry_size);
    return 0;
}


/** Generate a checkpoint and save it to disk file. */
void generate_checkpoint() {
    checkpoints ckpt;
    read_checkpoints(&ckpt);

    time_t cur_time;
    time(&cur_time);

    memcpy(ckpt[next_checkpoint].segment_bitmap, segment_bitmap, siezof(segment_bitmap));
    ckpt[next_checkpoint].count_inode = count_inode;
    ckpt[next_checkpoint].cur_block = cur_block;
    ckpt[next_checkpoint].cur_segment = cur_segment;
    ckpt[next_checkpoint].root_dir_inumber = root_dir_inumber;
    ckpt[next_checkpoint].timestamp = (int) cur_time;

    write_checkpoints(&ckpt);
    next_checkpoint = 1 - next_checkpoint;
}