#include "cleaner.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"
#include "wbcache.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <set>


bool _clean_thoroughly;

/** **************************************
 * Garbaged collection block-IO utilities.
 *    (names started with prefix "gc")
 * ***************************************/
/* Segment buffer specialized for garbage collection. */
int gc_cur_segment, gc_cur_block, gc_next_imap_index;
char gc_segment_buffer[SEGMENT_SIZE], gc_segment_bitmap[TOT_SEGMENTS];
int gc_inode_table[MAX_NUM_INODE];
inode gc_cached_inode_array[MAX_NUM_INODE]; 
char gc_file_buffer[FILE_SIZE];


/** Append a segment summary entry for a given block (in GC buffer). */
void gc_add_segbuf_summary(int block_index, int _i_number, int _direct_index) {
    int entry_size = sizeof(struct summary_entry);
    int buffer_offset = SUMMARY_OFFSET + block_index * entry_size;
    summary_entry blk_summary = {
        i_number     : _i_number,
        direct_index : _direct_index
    };
    memcpy(gc_segment_buffer + buffer_offset, &blk_summary, entry_size);
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
    if (_clean_thoroughly) {
        return gc_cur_segment+1;    // No need to %TOT_SEGMENTS, since we start from segment 0.
    } else {
        int next_free_segment = -1;
        for (int i=(gc_cur_segment+1)%TOT_SEGMENTS; i!=gc_cur_segment; i=(i+1)%TOT_SEGMENTS)
            if (gc_segment_bitmap[i] == 0) {
                next_free_segment = i;
                break;
            }
        return next_free_segment;   // This may be -1, indicating an error.
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
        // Segment buffer is full, and should be flushed to disk buffer.
        gc_add_segbuf_metadata();
        gc_write_segment(gc_segment_buffer, gc_cur_segment);
        gc_segment_bitmap[gc_cur_segment] = 1;

        int gc_next_segment = gc_get_next_free_segment();
        if (gc_next_segment == -1) {
            logger(WARN, "[WARNING] The file system is highly utilized, so that normal garbage collection fails.\n");
            logger(WARN, "[INFO] We will try to perform a thorough garbage collection.\n");
            /* Throw an exception: we should perform thorough GC. */
            throw ((int) -1);
        } else {
            memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
            gc_cur_segment = gc_next_segment;
            gc_cur_block = 0;
            gc_next_imap_index = 0;
        }

        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_next_segment);
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
    
    memcpy(gc_segment_buffer + buffer_offset, data, BLOCK_SIZE);        // Append inode block.
    gc_add_segbuf_summary(gc_cur_block, i_number, -1);                  // Append segment summary (use -1 as index).
    gc_add_segbuf_imap(i_number, block_addr);                           // Append imap entry for this inode.
    gc_move_to_segment();                                               // Write back GC buffer if necessary.

    return block_addr;
}

/** Remove an existing inode again (due to GC). */
void gc_remove_inode(int i_number) {
    if (DEBUG_GC_BLOCKIO)
        logger(DEBUG, "Remove inode block again (due to GC). Written to imap: #%d.\n", gc_next_imap_index);

    gc_inode_table[i_number] = -1;
    gc_add_segbuf_imap(i_number, -1);
    memset(gc_cached_inode_array+i_number, 0, sizeof(struct inode));
    
    // Imap modification may also trigger GC segment writeback.
    // If GC segment buffer is full, it should be flushed to GC file buffer.
    if (gc_cur_block == DATA_BLOCKS_IN_SEGMENT-1 || gc_next_imap_index == DATA_BLOCKS_IN_SEGMENT) {
        // Segment buffer is full, and should be flushed to disk file.
        gc_add_segbuf_metadata();
        gc_write_segment(gc_segment_buffer, gc_cur_segment);
        gc_segment_bitmap[gc_cur_segment] = 1;

        int gc_next_segment = gc_get_next_free_segment();
        if (gc_next_segment == -1) {
            logger(WARN, "[WARNING] The file system is highly utilized, so that normal garbage collection fails.\n");
            logger(WARN, "[INFO] We will try to perform a thorough garbage collection.\n");
            /* Throw an exception: copy the back-up memory into disk file, and perform thorough gc. */
            throw ((int) -1);
        } else {
            memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
            gc_cur_segment = gc_next_segment;
            gc_cur_block = 0;
            gc_next_imap_index = 0;
        }

        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_next_segment);
    }
}

/* Compare functions for segment statistics structures. */
bool _util_compare(struct util_entry &a, struct util_entry &b) {
    return (a.count < b.count) || ((a.count == b.count) && (a.segment_number < b.segment_number));
}
bool _time_compare(struct time_entry &a, struct time_entry &b) {
    return (   (a.update_sec < b.update_sec) || ((a.update_sec == b.update_sec) && (a.update_nsec < b.update_nsec))
            || ((a.update_sec == b.update_sec) && (a.update_nsec == b.update_nsec) && (a.segment_number < b.segment_number)) );
}


/** A variant of "get_lock(data, block_addr)" that does not acquire lock. */
void gc_get_block(void* data, int block_addr) {
    int segment = block_addr / BLOCKS_IN_SEGMENT;
    int block = block_addr % BLOCKS_IN_SEGMENT;

    if (segment == cur_segment) {    // Data in segment buffer.
        int buffer_offset = block * BLOCK_SIZE;
        memcpy(data, segment_buffer + buffer_offset, BLOCK_SIZE);
    } else {    // Data in disk file.
        int file_handle = open(lfs_path, O_RDWR);
        int file_offset = block_addr * BLOCK_SIZE;
        int read_length = pread(file_handle, data, BLOCK_SIZE, file_offset);
        close(file_handle);
    }
}

/* Compact data blocks within a segment */
void gc_compact_data_blocks(summary_entry* seg_sum, int seg, std::set<int> &modified_inum) {
    block data;
    struct inode* cur_inode;
    for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
        int i_number = seg_sum[j].i_number;
        int dir_index = seg_sum[j].direct_index;
        int block_addr = seg*BLOCKS_IN_SEGMENT + j;

        // Caution: use a stronger test criterion for validity.
        // Note that segment summary may be wrong sometimes.
        if ((i_number <= 0) || (i_number >=MAX_NUM_INODE) || (inode_table[i_number] == -1)) continue;
        
        if (dir_index == -1) {  // Block j is an inode block.
            if (inode_table[i_number] == block_addr)
                modified_inum.insert(i_number);
        } else {                // Block j is a data block.
            get_inode_from_inum(cur_inode, i_number);
            if (cur_inode->direct[dir_index] == block_addr) {
                modified_inum.insert(i_number);

                gc_get_block(&data, block_addr);
                gc_cached_inode_array[i_number].direct[dir_index] = gc_new_data_block(&data, i_number, dir_index);
            }
        }
    }
}


// RULE OF THUMBS FOR AVOIDING MISTAKES:
// * Read data using non-GC APIs (from the original file or cache).
// * Write data to GC data structures only (especially, never change global cache).
void collect_garbage(bool clean_thoroughly) {
    gc_lock_holder zhymoyu;
    // Must flush and re-initialize cache in the first hand.
    flush_cache();
    init_cache();

    // We will do all garbage collection completely in memory.
    // This may also prevent writing inconsistent data into disk file.
    _clean_thoroughly = clean_thoroughly;
    if (_clean_thoroughly) {
        /* Initialize file buffer to 0 for thorough garbage collection. */
        memset(gc_file_buffer, 0, sizeof(gc_file_buffer));
        memset(gc_segment_bitmap, 0, sizeof(gc_segment_bitmap));

        struct superblock init_sblock = {
            tot_inodes   : TOT_INODES,
            tot_blocks   : BLOCKS_IN_SEGMENT * TOT_SEGMENTS,
            tot_segments : TOT_SEGMENTS,
            block_size   : BLOCK_SIZE,
            segment_size : SEGMENT_SIZE
        };
        memcpy(gc_file_buffer+SUPERBLOCK_ADDR, &init_sblock, SUPERBLOCK_SIZE);

        checkpoints ckpt;
        read_checkpoints(&ckpt);
        memcpy(gc_file_buffer+CHECKPOINT_ADDR, &ckpt, CHECKPOINT_SIZE);
    } else {
        /* Load data from disk file. */
        int file_handle = open(lfs_path, O_RDWR);
        int read_length = pread(file_handle, gc_file_buffer, FILE_SIZE, 0);
        close(file_handle);

        memcpy(gc_segment_bitmap, segment_bitmap, sizeof(segment_bitmap));
    }
    
    /* Copy a GC version of all cached data in memory. */
    memset(gc_segment_buffer, 0, sizeof(gc_segment_buffer));
    gc_cur_segment     = 0;
    gc_cur_block       = 0;
    gc_next_imap_index = 0;
    memcpy(gc_inode_table, inode_table, sizeof(inode_table));
    memcpy(gc_cached_inode_array, cached_inode_array, sizeof(cached_inode_array));

    segment_summary seg_sum;
    struct inode* cur_inode;
    inode_map seg_imap;

    /* Initialize segment statistics for garbage collection. */
    util_entry utilization[TOT_SEGMENTS];
    time_entry timestamp[TOT_SEGMENTS];
    memset(utilization, 0, sizeof(utilization));
    memset(timestamp, 0, sizeof(timestamp));


    /* Calculate segment utilization (only for normal garbage collection). */
    if (!_clean_thoroughly) {
        for (int i=0; i<TOT_SEGMENTS; i++) {
            utilization[i].segment_number = i;
            if (segment_bitmap[i] == 0) {  // Free blocks are marked with -1 utilization.
                utilization[i].count = -1;
            } else {
                memcpy(&seg_sum, &cached_segsum[i], sizeof(seg_sum));
                for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
                    int i_number = seg_sum[j].i_number;
                    int dir_index = seg_sum[j].direct_index;
                    int block_addr = i*BLOCKS_IN_SEGMENT + j;

                    // Caution: use a stronger test criterion for validity.
                    // Note that segment summary may be wrong sometimes.
                    if ((i_number <= 0) || (i_number >= MAX_NUM_INODE) || (inode_table[i_number] == -1)) continue;

                    if (dir_index == -1) {  // Block j is an inode block.
                        if (inode_table[i_number] == block_addr)
                            utilization[i].count++;
                    } else {                // Block j is a data block.
                        get_inode_from_inum(cur_inode, i_number);
                        if (cur_inode->direct[dir_index] == block_addr)
                            utilization[i].count++;
                    }
                }
            }
        }

        std::sort(utilization, utilization+TOT_SEGMENTS, _util_compare);
        
        // Print debug information.
        if (DEBUG_GARBAGE_COL)
            print_util_stat(utilization);
    }

    /* Select next free segment to store blocks that are still alive.
     * Here we directly access the inode array, and record all modified inodes. */
    std::set<int> modified_inum;
    modified_inum.clear();
    if ((!_clean_thoroughly) && (utilization[0].count == -1)) {
        // If there are still free segments, we shall only clean segments with low utilizations.
        gc_cur_segment = utilization[0].segment_number;
        gc_cur_block = 0;
        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_cur_segment);
        
        // Step 0: determine the start point and end point for cleaning.
        // Will clean segments indexed [i_st, i_ed) in utilization array. 
        int i_st = 0;
        while (utilization[i_st].count == -1) i_st++;
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
        // After compaction, mark the segment to be empty.
        for (int i=i_st; i<i_ed; i++) {
            int seg = utilization[i].segment_number;
            memcpy(&seg_sum, &cached_segsum[seg], sizeof(seg_sum));
            if (DEBUG_GARBAGE_COL)
                logger(DEBUG, ">>> Cleaning segment %d.\n", seg);

            // Compact live data blocks.
            gc_compact_data_blocks(seg_sum, seg, modified_inum);

            // Record deleted inodes.
            read_segment_imap(&seg_imap, seg);
            for (int j=0; j<DATA_BLOCKS_IN_SEGMENT; j++) {
                if (seg_imap[j].inode_block == -1)
                    modified_inum.insert(seg_imap[j].i_number);
            }

            // Evict the segment (in GC buffer).
            memset(gc_file_buffer + seg*SEGMENT_SIZE, 0, SEGMENT_SIZE);
            gc_segment_bitmap[seg] = 0;
        }

        // Step 2: write back cached inodes after updating all data blocks.
        std::set<int>::iterator iter = modified_inum.begin();
        while (iter != modified_inum.end()) {
            int i_number = *iter;
            if (inode_table[i_number] == -1) {
                gc_remove_inode(i_number);
            } else {
                cur_inode = &gc_cached_inode_array[i_number];
                gc_inode_table[i_number] = gc_new_inode_block(cur_inode);
            }
            iter++;
        }

        logger(WARN, "[INFO] Successfully finished normal garbage collection.\n");
    } else {
        // If there are no free segments left, we should perform a thorough clean-up.
        // Now it is better to rewrite from timestamp order into sequential order.
        
        /* Retrieve segment timestamps. */
        memset(timestamp, 0, sizeof(timestamp));
        segment_metadata seg_metadata;
        for (int i=0; i<TOT_SEGMENTS; i++) {
            read_segment_metadata(&seg_metadata, i);
            timestamp[i].segment_number = i;
            timestamp[i].update_sec     = seg_metadata.update_sec;
            timestamp[i].update_nsec    = seg_metadata.update_nsec;
        }

        std::sort(timestamp, timestamp+TOT_SEGMENTS, _time_compare);

        /* Print debug information. */
        if (DEBUG_GARBAGE_COL)
            print_time_stat(timestamp);
        
        /* Perform a thorough garbage collection. */
        gc_cur_segment = 0;
        gc_cur_block = 0;
        if (DEBUG_GARBAGE_COL)
            logger(DEBUG, "* Start dumping GC buffer to segment %d.\n", gc_cur_segment);


        // Step 1: identify all live data blocks, and record inodes to be updated.
        // Note that deleted inodes may need to be re-deleted.
        for (int i=0; i<TOT_SEGMENTS; i++) {
            int seg = timestamp[i].segment_number;
            memcpy(&seg_sum, &cached_segsum[seg], sizeof(seg_sum));
            if (DEBUG_GARBAGE_COL)
                logger(DEBUG, ">>> Cleaning segment %d.\n", seg);

            // Compact live data blocks.
            gc_compact_data_blocks(seg_sum, seg, modified_inum);

            // There is no need to append deleted blocks or clean segments, 
            // since all deleted data automatically vanish from the GC buffer.
            // Remember that we start from an empty file buffer in thorough mode.
        }

        // Step 2: write back cached inodes after updating all data blocks.
        std::set<int>::iterator iter = modified_inum.begin();
        while (iter != modified_inum.end()) {
            int i_number = *iter;
            if (inode_table[i_number] != -1) {    // Only append undeleted inodes.
                cur_inode = &gc_cached_inode_array[i_number];
                gc_inode_table[i_number] = gc_new_inode_block(cur_inode);
            }
            iter++;
        }

        logger(WARN, "[INFO] Successfully finished thorough garbage collection.\n");
    }

    /* Write back to disk file */
    // (1) force the last segment into file buffer.
    gc_add_segbuf_metadata();    
    gc_write_segment(gc_segment_buffer, gc_cur_segment);
    gc_segment_bitmap[gc_cur_segment] = 1;

    // (2) replace segment bitmap with its GC version (checkpoint is generated later).
    memcpy(segment_bitmap, gc_segment_bitmap, sizeof(segment_bitmap));

    // (3) Write the cleaned data back to file.
    int file_handle = open(lfs_path, O_RDWR);
    int write_length = pwrite(file_handle, gc_file_buffer, FILE_SIZE, 0);
    close(file_handle);


    /* Update all GC version of temporary buffers by copying back. */
    // (1) copy GC segment buffer (may contain data and free blocks) back.
    memcpy(segment_buffer, gc_segment_buffer, sizeof(gc_segment_buffer));
    cur_segment     = gc_cur_segment;
    cur_block       = gc_cur_block;
    next_imap_index = gc_next_imap_index;

    // (2) update GC versions of cached (latest) inode information.
    memcpy(inode_table, gc_inode_table, sizeof(inode_table));
    memcpy(cached_inode_array, gc_cached_inode_array, sizeof(cached_inode_array));

    // (3) update segment summary from disk file.
    for (int seg=0; seg<TOT_SEGMENTS; seg++) {
        read_segment_summary(&seg_sum, seg);    
        memcpy(&cached_segsum[seg], &seg_sum, sizeof(seg_sum));
    }

    if (DEBUG_GARBAGE_COL)
        logger(DEBUG, "* Current buffer pointer at (segment %d, block %d), with next_imap_index = %d.\n", cur_segment, cur_block, next_imap_index);
}