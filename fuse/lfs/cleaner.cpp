#include "cleaner.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <set>


/** **************************************
 * Garbaged collection block-IO utilities.
 *    (names started with prefix "gc")
 * ***************************************/
/* Segment buffer specialized for garbage collection. */
int gc_cur_segment, gc_cur_block, gc_next_imap_index;
char gc_segment_buffer[SEGMENT_SIZE];
char gc_file_buffer[FILE_SIZE];

/* Auxiliary global variables. */
bool _clean_thoroughly, _seq_write;
time_entry timestamp[TOT_SEGMENTS];
int ts_pointer;

/** Append a segment summary entry for a given block (in GC buffer). */
void gc_add_segbuf_summary(int block_index, int _i_number, int _direct_index) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : _i_number,
        direct_index : _direct_index
    };
    memcpy(gc_segment_buffer + buffer_offset, &blk_summary, entry_size);
    cached_segsum[gc_cur_segment][block_index] = blk_summary;
}

/** Append a imap entry for a given inode block (in GC buffer). */
void gc_add_segbuf_imap(int _i_number, int _block_addr) {
    int entry_size = sizeof(struct imap_entry);
    int buffer_offset = IMAP_OFFSET + gc_next_imap_index * entry_size;
    imap_entry blk_imentry = {
        i_number     : _i_number,
        inode_block  : _block_addr
    };
    memcpy(gc_segment_buffer + buffer_offset, &blk_imentry, entry_size);
    gc_next_imap_index++;
}

/** Append metadata for a segment (in GC buffer). */
void gc_add_segbuf_metadata() {
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    segment_metadata seg_metadata = {
        update_sec  : cur_time.tv_sec,
        update_nsec : cur_time.tv_nsec,
        cur_block   : gc_cur_block
    };

    memcpy(gc_segment_buffer + SEGMETA_OFFSET, &seg_metadata, SEGMETA_SIZE);
}


/** Return the index of next free segment, or -1 if the disk is full. */
int gc_get_next_free_segment() {
    if (_seq_write) {
        return gc_cur_segment+1;
    } else {
        if (_clean_thoroughly) {
            while (timestamp[ts_pointer].segment_number != gc_cur_segment)
                ts_pointer++;
            ts_pointer++;
            gc_cur_segment = timestamp[ts_pointer].segment_number;
        } else {
            int next_free_segment = -1;
            for (int i=(gc_cur_segment+1)%TOT_SEGMENTS; i!=gc_cur_segment; i=(i+1)%TOT_SEGMENTS)
                if (segment_bitmap[i] == 0) {
                    next_free_segment = i;
                    break;
                }
            return next_free_segment;
        }
    }
}

/** Write to segment buffer (in cache). */
void gc_write_segment(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE;
    memcpy(gc_file_buffer+file_offset, buf, SEGMENT_SIZE);
}

/** Increment cur_block, and flush GC buffer if it is full. */
void gc_move_to_segment() {
    if (gc_cur_block == DATA_BLOCKS_IN_SEGMENT-1 || gc_next_imap_index == DATA_BLOCKS_IN_SEGMENT) {
        // Segment buffer is full, and should be flushed to disk file.
        gc_add_segbuf_metadata();
        gc_write_segment(gc_segment_buffer, gc_cur_segment);
        segment_bitmap[gc_cur_segment] = 1;

        int next_segment = gc_get_next_free_segment();
        if (next_segment == -1) {
            logger(WARN, "[WARNING] The file system is highly utilized, so that normal garbage collection fails.\n");
            logger(WARN, "[WARNING] We will try to perform a thorough garbage collection.\n");
            /* Throw an exception: copy the back-up memory into disk file, and perform thorough gc. */
            throw (-1);
        } else {
            memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
            gc_cur_segment = next_segment;
            gc_cur_block = 0;
            gc_next_imap_index = 0;
        }

        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", next_segment);
    } else {    // Segment buffer is not full yet.
        gc_cur_block++;
    }
}

/** Create a new data block into GC buffer. */
int gc_new_data_block(void* data, int i_number, int direct_index) {
    int buffer_offset = gc_cur_block * BLOCK_SIZE;
    int block_addr = gc_cur_segment * BLOCKS_IN_SEGMENT + gc_cur_block;

    if (DEBUG_GC_BLOCKIO)
        logger(DEBUG, "(GC) add data block at (segment %d, block %d). Next imap: #%d.\n", gc_cur_segment, gc_cur_block, gc_next_imap_index);
    
    memcpy(gc_segment_buffer + buffer_offset, data, BLOCK_SIZE);    // Append data block.
    gc_add_segbuf_summary(gc_cur_block, i_number, direct_index);    // Append segment summary.
    gc_move_to_segment();                                           // Write back GC buffer if necessary.
    
    return block_addr;
}

/** Create a new inode block into GC buffer. */
int gc_new_inode_block(struct inode* data) {
    int i_number = data->i_number;
    int buffer_offset = gc_cur_block * BLOCK_SIZE;
    int block_addr = gc_cur_segment * BLOCKS_IN_SEGMENT + gc_cur_block;

    if (DEBUG_GC_BLOCKIO)
        logger(DEBUG, "(GC) add inode block at (segment %d, block %d). Write to imap: #%d.\n", gc_cur_segment, gc_cur_block, gc_next_imap_index);
    
    memcpy(gc_segment_buffer + buffer_offset, data, BLOCK_SIZE);    // Append inode block.
    gc_add_segbuf_summary(gc_cur_block, i_number, -1);              // Append segment summary (use -1 as index).
    gc_add_segbuf_imap(i_number, block_addr);                       // Append imap entry for this inode,
    inode_table[i_number] = block_addr;                             //   update inode_table,
    memcpy(cached_inode_array+i_number, data, sizeof(struct inode));                           //   and update cached inode array.
    gc_move_to_segment();                                           // Write back GC buffer if necessary.

    return block_addr;
}


/* Compare functions for segment statistics structures. */
bool _util_compare(struct util_entry &a, struct util_entry &b) {
    return (a.count < b.count) || ((a.count == b.count) && (a.segment_number < b.segment_number));
}
bool _time_compare(struct time_entry &a, struct time_entry &b) {
    return (a.update_sec < b.update_sec) || ((a.update_sec == b.update_sec) && (a.update_nsec < b.update_nsec));
}


void gc_compact_data_blocks(summary_entry* seg_sum, int seg, std::set<int> &modified_inum) {
    block data;
    struct inode* cur_inode;
    for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
        int i_number = seg_sum[j].i_number;
        int dir_index = seg_sum[j].direct_index;
        int block_addr = seg*BLOCKS_IN_SEGMENT + j;

        if ((i_number == 0) || (inode_table[i_number] == -1)) continue;
        if (dir_index == -1) {
            if (inode_table[i_number] == block_addr)
                modified_inum.insert(i_number);
        } else {
            get_inode_from_inum(cur_inode, i_number);
            if (cur_inode->direct[dir_index] == block_addr) {
                modified_inum.insert(i_number);
                
                get_block(&data, block_addr);
                cached_inode_array[i_number].direct[dir_index] = gc_new_data_block(&data, i_number, dir_index);
            }
        }
    }
}

void collect_garbage(bool clean_thoroughly, bool sequential_write) {
    // We will do all garbage collection completely in memory.
    // This may also prevent writing inconsistent data into disk file.
    
    _clean_thoroughly = clean_thoroughly;
    _seq_write = clean_thoroughly && sequential_write;
    if (_seq_write) {
        /* Initialize file buffer to 0. */
        memset(gc_file_buffer, 0, sizeof(gc_file_buffer));

        struct superblock init_sblock = {
            tot_inodes   : TOT_INODES,
            tot_blocks   : BLOCKS_IN_SEGMENT * TOT_SEGMENTS,
            tot_segments : TOT_SEGMENTS,
            block_size   : BLOCK_SIZE,
            segment_size : SEGMENT_SIZE
        };
        memcpy(gc_file_buffer+SUPERBLOCK_ADDR, &init_sblock, SUPERBLOCK_SIZE);
    } else {
        /* Load data from disk file. */
        int file_handle = open(lfs_path, O_RDWR);
        int read_length = pread(file_handle, gc_file_buffer, FILE_SIZE, 0);
        close(file_handle);
    }
    

    memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
    gc_cur_segment     = 0;
    gc_cur_block       = 0;
    gc_next_imap_index = 0;

    segment_summary seg_sum;
    struct inode* cur_inode;

    /* Calculate segment utilization. */
    util_entry utilization[TOT_SEGMENTS];
    memset(utilization, 0, sizeof(utilization));
    if (!clean_thoroughly) {
        for (int i=0; i<TOT_SEGMENTS; i++) {
            utilization[i].segment_number = i;
            memcpy(&seg_sum, &cached_segsum[i], sizeof(seg_sum));
            for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
                int i_number = seg_sum[j].i_number;
                int dir_index = seg_sum[j].direct_index;
                int block_addr = i*BLOCKS_IN_SEGMENT + j;

                if ((i_number == 0) || (inode_table[i_number] == -1)) continue;
                if (dir_index == -1) {
                    if (inode_table[i_number] == block_addr)
                        utilization[i].count++;
                } else {
                    get_inode_from_inum(cur_inode, i_number);
                    if (cur_inode->direct[dir_index] == block_addr)
                        utilization[i].count++;
                }
            }
        }

        std::sort(utilization, utilization+TOT_SEGMENTS, _util_compare);

        // Mark segments with 0 utilization as empty.
        for (int i=0; utilization[i].count == 0; i++)
            segment_bitmap[utilization[i].segment_number] = 0;
        
        // Print debug information.
        if (DEBUG_GARBAGE_COL)
            print_util_stat(utilization);
    }

    /* Select next free segment to store blocks that are still alive.
     * Here we directly access the inode array, and record all modified inodes. */
    std::set<int> modified_inum;
    if ((!clean_thoroughly) && (utilization[0].count == 0)) {
        // If there are still free segments, we shall only clean segments with low utilizations.
        gc_cur_segment = utilization[0].segment_number;
        gc_cur_block = 0;
        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_cur_segment);
        
        // Step 0: determine the start point and end point for cleaning.
        int i_st = 0;
        while (utilization[i_st].count == 0) i_st++;
        int i_ed = i_st;
        while ((utilization[i_ed].count <= CLEAN_BELOW_UTIL) && (i_ed < TOT_SEGMENTS)) i_ed++;
        if (i_ed-i_st < CLEAN_NUM) {
            if (CLEAN_NUM > TOT_SEGMENTS-i_st) {
                i_ed = TOT_SEGMENTS;
            } else {
                i_ed = i_st + CLEAN_NUM;
            }
        }

        // Step 1: identify all live data blocks, and record inodes to be updated.
        for (int i=i_st; i<i_ed; i++) {
            int seg = utilization[i].segment_number;
            memcpy(&seg_sum, &cached_segsum[seg], sizeof(seg_sum));
            segment_bitmap[seg] = 0;

            if (DEBUG_GARBAGE_COL)
                logger(DEBUG, ">>> Cleaning segment %d.\n", seg);
            gc_compact_data_blocks(seg_sum, seg, modified_inum);
        }

        // Step 2: write back cached inodes after updating all data blocks.
        std::set<int>::iterator iter = modified_inum.begin();
        while (iter != modified_inum.end()) {
            cur_inode = &cached_inode_array[*iter];
            inode_table[cur_inode->i_number] = gc_new_inode_block(cur_inode);
            iter++;
        }

        logger(WARN, "[WARNING] Successfully finished normal garbage collection.\n");
    } else {
        // If there are no free segments left, we should perform a thorough clean-up.
        // Now it is better to rewrite from timestamp order into sequential order.
        
        /* Retrieve segment timestamps. */
        memset(timestamp, 0, sizeof(timestamp));
        ts_pointer = 0;

        segment_metadata seg_metadata;
        for (int i=0; i<TOT_SEGMENTS; i++) {
            read_segment_metadata(&seg_metadata, i);
            timestamp[i].segment_number = i;
            timestamp[i].update_sec     = seg_metadata.update_sec;
            timestamp[i].update_nsec    = seg_metadata.update_nsec;
        }

        std::sort(timestamp, timestamp+TOT_SEGMENTS, _time_compare);
        // Print debug information.
        if (DEBUG_GARBAGE_COL)
            print_time_stat(timestamp);
        
        /* Perform a thorough garbage collection. */
        if (_seq_write) {
            gc_cur_segment = 0;
        } else {
            gc_cur_segment = timestamp[0].segment_number;
        }
        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_cur_segment);
        gc_cur_block = 0;

        // Step 1: identify all live data blocks, and record inodes to be updated.
        for (int i=0; i<TOT_SEGMENTS; i++) {
            int seg = timestamp[i].segment_number;
            memcpy(&seg_sum, &cached_segsum[seg], sizeof(seg_sum));
            segment_bitmap[seg] = 0;

            if (DEBUG_GARBAGE_COL)
                logger(DEBUG, ">>> Cleaning segment %d.\n", seg);
            gc_compact_data_blocks(seg_sum, seg, modified_inum);
        }

        // Step 2: write back cached inodes after updating all data blocks.
        std::set<int>::iterator iter = modified_inum.begin();
        while (iter != modified_inum.end()) {
            cur_inode = &cached_inode_array[*iter];
            inode_table[cur_inode->i_number] = gc_new_inode_block(cur_inode);
            iter++;
        }

        logger(WARN, "[WARNING] Successfully finished thorough garbage collection.\n");
    }

    /* Write back to disk file */
    int file_handle = open(lfs_path, O_RDWR);
    int write_length = pwrite(file_handle, gc_file_buffer, FILE_SIZE, 0);
    close(file_handle);

    /* We should copy GC buffer (still data remaining) to the main buffer. */
    memset(segment_buffer, 0, sizeof(segment_buffer));
    memcpy(segment_buffer, gc_segment_buffer, sizeof(gc_segment_buffer));
    cur_segment     = gc_cur_segment;
    cur_block       = gc_cur_block;
    next_imap_index = gc_next_imap_index;

    if (DEBUG_GARBAGE_COL)
        logger(DEBUG, "* Current buffer pointer at (segment %d, block %d), with next_imap_index = %d.\n", cur_segment, cur_block, next_imap_index);
}