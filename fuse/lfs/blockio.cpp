#include "blockio.h"

#include "logger.h"
#include "utility.h"

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <cstring>


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
        memcpy(data, segment_buffer + buffer_offset, BLOCK_SIZE);
    } else {    // Data in disk file.
        read_block(data, block_addr);
    }
    return 0;
}


/** Create a new block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @return flag: indicating whether appending is successful.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_block(void* data) {
    int buffer_offset = cur_block * BLOCK_SIZE;
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);
    
    if (cur_block == BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        write_segment(segment_buffer, cur_segment);
        cur_segment++;
        cur_block = 0;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }
    return 0;
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