#include "cleaner.h"

#include "logger.h"
#include "print.h"
#include "utility.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <set>

/* A struct to record utilization of each segment. */
struct util_entry {
    int segment_number;
    int count;
}
bool _compare(struct util_entry &a, struct util_entry &b) {
    return a.count < b.count;
}

/* Segment buffer specialized for garbage collection. */
int gc_cur_segment, gc_cur_block, gc_next_imap_index;
char gc_segment_buffer[SEGMENT_SIZE];

/** Increment cur_block, and flush segment buffer if it is full. */
void gc_move_to_segment() {
    if (gc_cur_block == DATA_BLOCKS_IN_SEGMENT-1 || gc_next_imap_index == DATA_BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        gc_add_segbuf_metadata();
        write_segment(gc_segment_buffer, gc_cur_segment);
        segment_bitmap[gc_cur_segment] = 1;

        int next_segment = gc_get_next_free_segment();  // ERROR!
        if (next_segment == -1) {
            is_full = true;
            generate_checkpoint();  // A checkpoint is necessary for correct recovery.

            logger(WARN, "[WARNING] The file system is already full.\n* Please run garbage collection to release space.\n");
            return;
        } else {
            memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
            gc_cur_segment = next_segment;
            gc_cur_block = 0;
            gc_next_imap_index = 0;
        }
    } else {    // Segment buffer is not full yet.
        gc_cur_block++;
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

    if (DEBUG_BLOCKIO)
        logger(DEBUG, "Add data block at (segment %d, block %d). Next imap: #%d.\n", cur_segment, cur_block, next_imap_index);

    acquire_writer_lock();
        if (!is_full) {
            // Append data block.
            memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

            // Append segment summary for this block.
            add_segbuf_summary(cur_block, i_number, direct_index);
            
            // Write back segment buffer if necessary.
            move_to_segment();
        }
    release_writer_lock();
    
    return block_addr;
}


/** Create a new inode block into the segment buffer.
 * @param  data: pointer of the inode to be appended.
 * @return block_addr: global block address of the new block.
 * Note that when the segment buffer is full, we have to write it back into disk file. */
int new_inode_block(struct inode* data) {
    int i_number = data->i_number;
    int buffer_offset = cur_block * BLOCK_SIZE;
    int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;

    if (DEBUG_BLOCKIO)
        logger(DEBUG, "Add inode block at (segment %d, block %d). Write to imap: #%d.\n", cur_segment, cur_block, next_imap_index);

    acquire_writer_lock();
        if (!is_full) {
            // Append inode block.
            memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

            // Append segment summary for this block.
            // [CAUTION] We use index -1 to represent an inode, rather than a direct[] pointer.
            add_segbuf_summary(cur_block, i_number, -1);

            // Append imap entry for this inode, and update inode_table.
            add_segbuf_imap(i_number, block_addr);
            inode_table[i_number] = block_addr;
            cached_inode_array[i_number] = *data;
            
            // Write back segment buffer if necessary.
            move_to_segment();
        }
    release_writer_lock();

    return block_addr;
}


/** Append a segment summary entry for a given block.
 * @param  block_index: index of the new block.
 * @param  _i_number: i_number of the file (that the data belongs to).
 * @param  _direct_index: the index of direct[] in that inode, pointing to the new block. 
 * [CAUTION] We use index -1 to represent an inode, which is NOT a direct[] pointer. */
void add_segbuf_summary(int block_index, int _i_number, int _direct_index) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : _i_number,
        direct_index : _direct_index
    };
    memcpy(segment_buffer + buffer_offset, &blk_summary, entry_size);
    cached_segsum[cur_segment][cur_block] = blk_summary;
}


/** Append a imap entry for a given inode block.
 * @param  i_number: i_number of the added inode.
 * @param  block_addr: global block address of the added inode. */
void add_segbuf_imap(int _i_number, int _block_addr) {
    int entry_size = sizeof(struct imap_entry);
    int buffer_offset = IMAP_OFFSET + next_imap_index * entry_size;
    imap_entry blk_imentry = {
        i_number     : _i_number,
        inode_block  : _block_addr
    };
    memcpy(segment_buffer + buffer_offset, &blk_imentry, entry_size);
    next_imap_index++;
}


/** Append metadata for a segment. */
void add_segbuf_metadata() {
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    segment_metadata seg_metadata = {
        update_time : cur_time.tv_sec,
        cur_block   : cur_block
    };

    memcpy(segment_buffer + SEGMETA_OFFSET, &seg_metadata, SEGMETA_SIZE);
}


/** Create a new data block into the segment buffer (used for garbage collection only). */
int gc_paste_block(void* data, int _i_number, int _direct_index) {
    int buffer_offset = gc_cur_block * BLOCK_SIZE;
    int block_addr = gc_cur_segment * BLOCKS_IN_SEGMENT + gc_cur_block;

    if (DEBUG_GARBCOL)
        logger(DEBUG, "Add data block at (segment %d, block %d). Next imap: #%d.\n", gc_cur_segment, gc_cur_block, gc_next_imap_index);

    // Append data block.
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);
    
    // Append segment summary for this block.
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : _i_number,
        direct_index : _direct_index
    };
    memcpy(segment_buffer + buffer_offset, &blk_summary, entry_size);
    cached_segsum[cur_segment][cur_block] = blk_summary;
    
    // Write back segment buffer if necessary.
    move_to_segment();
    
    return block_addr;
}

void collect_garbage() {
    // Calculate segment utilization.
    util_entry utilization[TOT_SEGMENTS];
    memset(utilization, 0, sizeof(utilization));

    segment_summary seg_sum;
    struct inode cur_inode;
    for (int i=0; i<TOT_SEGMENTS; i++) {
        utilization[i].segment_number = i;
        memcpy(&seg_sum, &cached_segsum[i], sizeof(seg_sum));
        for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
            int i_number = seg_sum[j].i_number;
            int dir_index = seg_sum[j].direct_index;
            int block_addr =  i*BLOCKS_IN_SEGMENT + j;

            if (i_number > 0) continue;
            if (dir_index == -1) {
                if (inode_table[i_number] == block_addr)
                    utilization[i].count++;
            } else {
                get_inode_from_inum(&cur_inode, i_number);
                if (cur_inode.direct[dir_index] == block_addr)
                    utilization[i].count++;
            }
        }
    }

    std::sort(utilization, utilization+TOT_SEGMENTS, _compare);


    // Select next free segment to store blocks that are still alive.
    // Here we directly access the inode array, and record all modified inodes.
    std::set<int> modified_inode;
    block data;
    if (utilization[0].count == 0) {
        // If there are still free segments, we shall only clean segments with low utilizations.
        cur_segment = utilization[0].segment_number;
        cur_block = 0;
        
        int idx = 0;
        while (utilization[idx] == 0) i++;
        for (int i=0; i<CLEAN_NUM; i++) {
            int seg = utilization[idx].segment_number;
            memcpy(&seg_sum, &cached_segsum[i], sizeof(seg_sum));
            for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
                int i_number = seg_sum[j].i_number;
                int dir_index = seg_sum[j].direct_index;
                int block_addr =  i*BLOCKS_IN_SEGMENT + j;

                if (i_number > 0) continue;
                if (dir_index == -1) {
                    if (inode_table[i_number] == block_addr)
                        modified_inode.insert(i_number);
                } else {
                    if (!cached_inode_valid[i_number]) {
                        get_block(&cur_inode, inode_table[i_number]);
                        cached_inode_array[i_number] = &cur_inode;
                        cached_inode_valid[i_number] = true;
                    }
                    
                    get_block(&data, block_addr);
                    cached_inode_array[i_number].direct[dir_index] = gc_new_data_block(&data, i_number, dir_index);
                }
            }

            idx++;
        }
    } else {
        // If there are no free segments left, we should perform a thorough clean-up.
        // Now we have to start writing to lowest-utilized segments.
    }


    

}