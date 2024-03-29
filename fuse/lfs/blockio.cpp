#include "blockio.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "cleaner.h"
#include "system.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fuse.h>
#include "wbcache.h"

/** Retrieve block according to the block address.
 * @param  data: pointer of return data.
 * @param  block_addr: block address.
 * Note that the block may be in segment buffer, or in disk file. */
void get_block(void* data, int block_addr) {
    if (GC_CONCURRENCY && is_doing_gc) { 
        // When doing GC, retrieve from either disk file (addr > 0) or pending buffer (addr <= -3).
        // Note that GC is only triggered when segment buffer is empty, and when cache is disabled.
        if (block_addr >= 0) {
            read_block(data, block_addr);
        } else if (block_addr <= -3) {
            memcpy(data, pending_block_buffer[-block_addr-3].data, BLOCK_SIZE);
        } else {
            logger(DEBUG, "[Fatal Error] Invalid block address %d (addresses -1 and -2 are reserved).\n", block_addr);
            exit(-1);
        }
        return;
    }

    acquire_segment_lock();
        // When not doing GC, retrieve from either segment buffer or disk file (maybe through cache).
        int segment = block_addr / BLOCKS_IN_SEGMENT;
        int block = block_addr % BLOCKS_IN_SEGMENT;

        if (segment == cur_segment) {    // Data in segment buffer.
            int buffer_offset = block * BLOCK_SIZE;

            memcpy(data, segment_buffer + buffer_offset, BLOCK_SIZE);
        } else {    // Data in disk file.
            if (USE_CACHE)
                read_block_through_cache(data, block_addr);
            else
                read_block(data, block_addr);
        }
    release_segment_lock();
}

/** Retrieve block according to the i_number of inode block.
 * @param  inode_data: pointer of returned inode.
 * @param  i_number: i_number of block.
 * @return flag: 0 on success, standard negative error codes on error.
 * Note that the block may be in inode cache, segment buffer, or disk file. */
void get_inode_from_inum(struct inode* &inode_data, int i_number) {
    inode_data = cached_inode_array+i_number;
    if (i_number != inode_data->i_number) {
        logger(ERROR, "[FATAL ERROR] Corrupt file system: inconsistent inode number in memory.\n");
        logger(ERROR, "* Should retrieve i_number %d, but get #%d from inode array.\n", i_number, inode_data->i_number);

        /* struct inode err_inode;
        memset(&err_inode, 0, sizeof(err_inode));
        get_block(&err_inode, inode_table[i_number]);
        print(&err_inode);
        exit(-1); */
        // For debugging only (not used in submission).
    }
}


/** Update garbage collection level based on time interval. */
bool get_garbcol_status(int level) {
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    if (level >= cur_garbcol_level) {
        last_garbcol_time = cur_time.tv_sec;
        return true;
    }
    else {
        if (cur_time.tv_sec - last_garbcol_time > GARBCOL_INTERVAL) {
            cur_garbcol_level = level;
            last_garbcol_time = cur_time.tv_sec;
            return true;
        } else {
            return false;
        }
    }
}


/** Retrieve the next free segment by searching segment_bitmap).
 * We also try to do garbage collection if full segments exceed CLEAN_THRESHOLD (80%).
 * @return flag: true if the disk is not full, and false otherwise. */
void get_next_free_segment() {
    bool isSequentialNext = true;
    if (allow_gc) {
        // Only check the possibility of GC when it is allowed (no recursive GC).

        // Count full segments for potential garbage collection.
        int count_full_segment = 0;
        for (int i=0; i<TOT_SEGMENTS; i++)
            count_full_segment += segment_bitmap[i];
        
        isSequentialNext = false;
        if (count_full_segment == TOT_SEGMENTS) {
            if (is_full) {
                logger(WARN, "\n[WARNING] The file system is full, and cannot make any further space.\n");
                logger(WARN, "[INFO] You may format the disk by deleting the disk file (lfs.data).\n");
                return;
            } else {
                get_garbcol_status(GARBCOL_LEVEL_100);
                logger(WARN, "\n\n[WARNING] The file system is completely full (100%% occpuied).\n");
                logger(WARN, "[INFO] We will run thorough garbage collection now for more disk space.\n");
                collect_garbage(true);
                generate_checkpoint();

                /* Recount the number of full segments to determine whether it is full. */
                int recount_full_segment = 0;
                for (int i=0; i<TOT_SEGMENTS; i++)
                    recount_full_segment += segment_bitmap[i];
                if ((recount_full_segment == TOT_SEGMENTS) && (cur_block >= BLOCKS_IN_SEGMENT / 2)) {
                    is_full = true;
                    logger(WARN, "\n[WARNING] The file system is full, and cannot make any further space.\n");
                    logger(WARN, "[INFO] You may format the disk by deleting the disk file (lfs.data).\n");
                    return;
                }
            }
        } else if (count_full_segment >= CLEAN_THORO_THRES) {
            bool isNecessary = get_garbcol_status(GARBCOL_LEVEL_96);
            if (isNecessary) {
                logger(WARN, "\n\n[WARNING] The file system is almost full (exceeding the 96%% threshold).\n");
                logger(WARN, "[INFO] We will run thorough garbage collection now for more disk space.\n");
                collect_garbage(true);
                generate_checkpoint();

                /* Recount the number of full segments to determine whether it is full. */
                int recount_full_segment = 0;
                for (int i=0; i<TOT_SEGMENTS; i++)
                    recount_full_segment += segment_bitmap[i];
                if (recount_full_segment >= CLEAN_THORO_FAIL) {
                    cur_garbcol_level = GARBCOL_LEVEL_100;
                    logger(WARN, "\n[WARNING] Thorough garbage collection fails to release much space (still >= 85%% full).\n");
                    logger(WARN, "[INFO] It is advisable to use a larger disk file for better performance.\n");
                }
            } else {
                isSequentialNext = true;
                logger(WARN, "\n\n[WARNING] The file system is almost full (exceeding the 96%% threshold).\n");
                logger(WARN, "[WARNING] However, garbage collection is unnecessary because it does not work well.\n");
            }
        } else if (count_full_segment >= CLEAN_THRESHOLD) {
            bool isNecessary = get_garbcol_status(GARBCOL_LEVEL_80);
            if (isNecessary) {
                logger(WARN, "\n\n[WARNING] The file system is largely full (exceeding the 80%% threshold).\n");
                logger(WARN, "[INFO] We will run normal garbage collection now for better performance.\n");

                try {
                    collect_garbage(false);
                } catch (int e) {
                    if (e == -1) {
                        logger(WARN, "\n[WARNING] The file system is highly utilized, so that normal garbage collection fails.\n");
                        logger(WARN, "[INFO] We will try to perform a thorough garbage collection instead.\n");

                        collect_garbage(true);
                    }
                }
                generate_checkpoint();

                /* Recount the number of full segments to determine whether it is full. */
                int recount_full_segment = 0;
                for (int i=0; i<TOT_SEGMENTS; i++)
                    recount_full_segment += segment_bitmap[i];
                if (recount_full_segment >= CLEAN_NORM_FAIL) {
                    cur_garbcol_level = GARBCOL_LEVEL_96;
                    logger(WARN, "\n[WARNING] Normal garbage collection fails to release much space (still >= 60%% full).\n");
                    logger(WARN, "[INFO] It is advisable to use a larger disk file for better performance.\n");
                }
            } else {
                isSequentialNext = true;
                logger(WARN, "\n\n[WARNING] The file system is almost full (exceeding the 80%% threshold).\n");
                logger(WARN, "[WARNING] However, garbage collection is unnecessary because it will not work well.\n");
            }
        } else {
            isSequentialNext = true;
        }
    }

    if (isSequentialNext) {
        // If the function proceeds here, the disk may still be full.
        // We have to select next free segment (whether we did garbage collection or not).
        int next_free_segment = -1;
        for (int i=(cur_segment+1)%TOT_SEGMENTS; i!=cur_segment; i=(i+1)%TOT_SEGMENTS)
            if (segment_bitmap[i] == 0) {
                next_free_segment = i;
                break;
            }
        
        if (next_free_segment == -1) {
            is_full = true;
            logger(WARN, "\n[WARNING] The file system is full, and cannot make any further space.\n");
            logger(WARN, "[INFO] You may format the disk by deleting the disk file (lfs.data).\n");
            return;
        }
        
        // Initialize segment buffer.
        memset(segment_buffer, 0, sizeof(segment_buffer));
        cur_segment     = next_free_segment;
        cur_block       = 0;
        next_imap_index = 0;
    }
}

/** Increment cur_block, and flush segment buffer if it is full. */
void move_to_segment() {
    if (is_full) {
        logger(WARN, "[WARNING] The file system is already full: please expand the disk size.\n");
        logger(WARN, "* Garbage collection fails because it cannot release any blocks.\n");
        return;
    }

    if (cur_block == DATA_BLOCKS_IN_SEGMENT-1 || next_imap_index == DATA_BLOCKS_IN_SEGMENT) {
        // Segment buffer is full, and should be flushed to disk file.
        add_segbuf_metadata();
        if (USE_CACHE)
            write_segment_through_cache(segment_buffer, cur_segment);
        else
            write_segment(segment_buffer, cur_segment);
        segment_bitmap[cur_segment] = 1;

        get_next_free_segment();
        segment_bitmap[cur_segment] = 1;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }
}

/** Create a new data block into the segment buffer.
 * @param  data: pointer of data to be appended.
 * @param  data_inode: inode that the data belongs to (may be head or non-head inodes).
 * @param  direct_index: the index of direct[] in that inode, pointing to the new block.
 * Note that when the segment buffer is full, we have to write it back into disk file. 
 * This function does not return block_addr, because block_addr should be updated before
 * potential garbage collection triggered by move_to_segment(), where addresses are changed. */
void new_data_block(void* data, struct inode* data_inode, int direct_index) {
    if (GC_CONCURRENCY && is_doing_gc) {
        // When doing GC, we should redirect the writing request to pending block buffer.
        struct pending_block pblock;
        pblock.i_number = data_inode->i_number;
        pblock.direct_index = direct_index;
        pblock.is_remove = false;
        memcpy(pblock.data, data, BLOCK_SIZE);

        pending_block_buffer.push_back(pblock);
        data_inode->direct[direct_index] = -pending_block_buffer.size()-2;
        return;
    }

    if (allow_gc) acquire_segment_lock();
        // When not doing GC, we should write to segment buffer as normal.
        int i_number = data_inode->i_number;
        int buffer_offset = cur_block * BLOCK_SIZE;
        int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;

        if (DEBUG_BLOCKIO)
            logger(DEBUG, "Add data block at (segment %d, block %d). Next imap: #%d.\n", cur_segment, cur_block, next_imap_index);

        if (!is_full) {
            // Append data block.
            memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

            // Append segment summary for this block.
            add_segbuf_summary(cur_block, i_number, direct_index);
            data_inode->direct[direct_index] = block_addr;
            
            // Write back segment buffer if necessary.
            move_to_segment();
        }
    if (allow_gc) release_segment_lock();
}


/** Create a new inode block into the segment buffer.
 * @param  data: pointer of the inode to be appended.
 * @return block_addr: global block address of the new block.
 * Note that when the segment buffer is full, we have to write it back into disk file.
 * This function does not return block_addr, because block_addr should be updated before
 * potential garbage collection triggered by move_to_segment(), where addresses are changed. */
void new_inode_block(struct inode* data) {
    if (GC_CONCURRENCY && is_doing_gc) {
        // When doing GC, we should redirect the writing request to pending block buffer.
        struct pending_block pblock;
        pblock.i_number = data->i_number;
        pblock.direct_index = -1;
        pblock.is_remove = false;
        memcpy(pblock.data, data, BLOCK_SIZE);

        pending_block_buffer.push_back(pblock);
        inode_table[data->i_number] = -pending_block_buffer.size()-2;
        return;
    }

    if (allow_gc) acquire_segment_lock();
        // When not doing GC, we should write to segment buffer as normal.
        int i_number = data->i_number;
        int buffer_offset = cur_block * BLOCK_SIZE;
        int block_addr = cur_segment * BLOCKS_IN_SEGMENT + cur_block;

        if (DEBUG_BLOCKIO)
            logger(DEBUG, "Add inode block at (segment %d, block %d). Write to imap: #%d.\n", cur_segment, cur_block, next_imap_index);

        if (is_full) {
            logger(WARN, "[WARNING] The file system is already full: please expand the disk size.\n");
            logger(WARN, "* Garbage collection fails because it cannot release any blocks.\n");
            if (allow_gc) release_segment_lock();
            return;
        }

        if (!is_full) {
            // Append inode block.
            memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);

            // Append segment summary for this block.
            // [CAUTION] We use index -1 to represent an inode, rather than a direct[] pointer.
            add_segbuf_summary(cur_block, i_number, -1);

            // Append imap entry for this inode, and update inode_table.
            add_segbuf_imap(i_number, block_addr);
            inode_table[i_number] = block_addr;
            memcpy(cached_inode_array+i_number, data, sizeof(struct inode));
            
            // Write back segment buffer if necessary.
            move_to_segment();
        }
    if (allow_gc) release_segment_lock();
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
    cached_segsum[cur_segment][block_index] = blk_summary;
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
        update_sec  : cur_time.tv_sec,
        update_nsec : cur_time.tv_nsec,
        cur_block   : cur_block
    };
    memcpy(segment_buffer + SEGMETA_OFFSET, &seg_metadata, SEGMETA_SIZE);
}


/** Initialize a new file by creating its inode.
 * @param  cur_inode: struct for the new inode (should be manually allocated before function call).
 * @param  _mode: type of the file (1 = file, 2 = dir; -1 = non-head).
 * @param  _permission: using UGO x RWX format in base-8 (e.g., 0777). 
 * [CAUTION] It is required to use malloc to create cur_inode (see file_add_data() below). */
void file_initialize(struct inode* &cur_inode, int _mode, int _permission) {
    acquire_segment_lock();
    acquire_counter_lock();
        if (count_inode >= MAX_NUM_INODE-1) {
            is_full = true;
            release_counter_lock();
            release_segment_lock();
            return;
        }
        count_inode++;
        cur_inode = cached_inode_array + count_inode;
        cur_inode->i_number = count_inode;

        // "-2" means a transient state, where an inode is created but not yet written to disk.
        // Note that this will never appear on disk (if LFS crashes before commitment, the inode is lost).
        inode_table[count_inode] = -2;

        cur_inode->mode         = _mode;
        cur_inode->num_links    = 1;
        cur_inode->fsize_byte   = 0;
        cur_inode->fsize_block  = 0;
        cur_inode->io_block     = 1;
        cur_inode->permission   = _permission;
        cur_inode->device       = USER_DEVICE;
        cur_inode->num_direct   = 0;
        memset(cur_inode->direct, -1, sizeof(cur_inode->direct));
        cur_inode->next_indirect = 0;

        struct fuse_context* user_info = fuse_get_context();  // Get information (uid, gid) of the user who calls LFS interface.
        cur_inode->perm_uid     = user_info->uid;
        cur_inode->perm_gid     = user_info->gid;

        // Update current inode time.
        struct timespec cur_time;
        clock_gettime(CLOCK_REALTIME, &cur_time);
        cur_inode->atime = cur_time;
        cur_inode->mtime = cur_time;
        cur_inode->ctime = cur_time;
    release_counter_lock();
    release_segment_lock();
}


/** Append to the new file by adding new data blocks (possibly storing full inodes in log).
 * @param  cur_inode: struct for the file inode.
 * @param  data: buffer for the file data block to be appended. */
void file_add_data(struct inode* &cur_inode, void* data) {
    // If the file is too large, another (pseudo) inode is necessary.
    if (cur_inode->num_direct == NUM_INODE_DIRECT) {
        // Create the next inode.
        struct inode* next_inode;
        file_initialize(next_inode, MODE_MID_INODE, cur_inode->permission);
        
        // Update current inode and commit it.
        struct timespec cur_time;
        clock_gettime(CLOCK_REALTIME, &cur_time);
        cur_inode->atime = cur_time;
        cur_inode->mtime = cur_time;
        cur_inode->ctime = cur_time;

        cur_inode->next_indirect = next_inode->i_number;
        new_inode_block(cur_inode);

        // Release current inode by replacing the pointer.
        cur_inode = next_inode;
    }

    new_data_block(data, cur_inode, cur_inode->num_direct);
    cur_inode->fsize_block++;
    cur_inode->num_direct++;
}


/** Modify an existing file by replacing a data block at given index.
 * @param  cur_inode: existing struct for the file inode.
 * [CAUTION] Inodes already in log cannot be directly modified. It must be read out by get_block() first.
 * @param  direct_index: index of the modification (w.r.t. direct[] of the inode).
 * @param  data: a new data block. */
void file_modify(struct inode* cur_inode, int direct_index, void* data) {
    if (direct_index >= cur_inode->num_direct) {
        logger(ERROR, "[ERROR] Cannot modify a block that does not exist yet. Request ind: %d\n", direct_index);
    }
    
    new_data_block(data, cur_inode, direct_index);
}


/** Remove an existing inode.
 * @param  i_number: i_number of an existing inode. */
void remove_inode(int i_number) {
    if (GC_CONCURRENCY && is_doing_gc) {
        // When doing GC, we should redirect the writing request to pending block buffer.
        struct pending_block pblock;
        pblock.i_number = i_number;
        pblock.direct_index = -1;
        pblock.is_remove = true;
        memset(pblock.data, 0, BLOCK_SIZE);

        pending_block_buffer.push_back(pblock);
        inode_table[i_number] = -1;
        return;
    }
    
    if (allow_gc) acquire_segment_lock();
        // When not doing GC, we should write to segment buffer as normal.
        if (DEBUG_BLOCKIO)
            logger(DEBUG, "Remove inode block. Written to imap: #%d.\n", next_imap_index);
        inode_table[i_number] = -1;
        add_segbuf_imap(i_number, -1);

        // Caution: we cannot set the inode to 0 here due to synchronization problems.
        // However, inode_table should be cleared for correct book-keeping.
        // memset(cached_inode_array+i_number, 0, sizeof(struct inode));
        
        // Imap modification may also trigger segment writeback.
        // If segment buffer is full, it should be flushed to disk file.
        if (cur_block == DATA_BLOCKS_IN_SEGMENT-1 || next_imap_index == DATA_BLOCKS_IN_SEGMENT) {
            add_segbuf_metadata();
            if (USE_CACHE)
                write_segment_through_cache(segment_buffer, cur_segment);
            else
                write_segment(segment_buffer, cur_segment);
            segment_bitmap[cur_segment] = 1;

            get_next_free_segment();
            segment_bitmap[cur_segment] = 1;
        }
    if (allow_gc) release_segment_lock();
}


/** Generate a checkpoint and save it to disk file. */
void generate_checkpoint() {
    checkpoints ckpt;
    read_checkpoints(&ckpt);

    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    memcpy(ckpt[next_checkpoint].segment_bitmap, segment_bitmap, sizeof(segment_bitmap));
    ckpt[next_checkpoint].is_full           = is_full;
    ckpt[next_checkpoint].count_inode       = count_inode;
    ckpt[next_checkpoint].cur_block         = cur_block;
    ckpt[next_checkpoint].cur_segment       = cur_segment;
    ckpt[next_checkpoint].next_imap_index   = next_imap_index;
    ckpt[next_checkpoint].timestamp_sec     = cur_time.tv_sec;
    ckpt[next_checkpoint].timestamp_nsec    = cur_time.tv_nsec;

    write_checkpoints(&ckpt);
    next_checkpoint = 1 - next_checkpoint;

    if (DEBUG_CKPT_REPORT)
        print(ckpt);
}