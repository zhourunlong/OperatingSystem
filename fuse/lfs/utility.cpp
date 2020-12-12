#include "utility.h"

#include "logger.h"
#include "blockio.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

char* lfs_path;
char segment_buffer[SEGMENT_SIZE];
char segment_bitmap[TOT_SEGMENTS];
int inode_table[MAX_NUM_INODE];
int count_inode, cur_segment, cur_block;
int next_checkpoint, next_imap_index;
struct timespec last_ckpt_update_time;
std::mutex global_lock;

/** **************************************
 * Block operations
 * @param  buf: buffer of any type, but should be of length BLOCK_SIZE;
 * @param  block_addr: block address (= seg * BLOCKS_IN_SEGMENT + blk).
 * @param  segment_addr: segment address (= seg).
 * @return length: actual length of reading / writing; -1 on error.
 * ****************************************/

/** Read a block into the buffer. */
int read_block(void* buf, int block_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = block_addr * BLOCK_SIZE;
    int read_length = pread(file_handle, buf, BLOCK_SIZE, file_offset);
    close(file_handle);
    return read_length;
}

/** Write a block into disk file (not recommended). */
int write_block(void* buf, int block_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = block_addr * BLOCK_SIZE;
    int write_length = pwrite(file_handle, buf, BLOCK_SIZE, file_offset);
    close(file_handle);
    return write_length;
}

/** Read a segment into the buffer (not usual). */
int read_segment(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE;
    int read_length = pread(file_handle, buf, SEGMENT_SIZE, file_offset);
    close(file_handle);
    return read_length;
}

/** Write a segment into disk file. */
int write_segment(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE;
    int write_length = pwrite(file_handle, buf, SEGMENT_SIZE, file_offset);
    close(file_handle);
    return write_length;
}



/** **************************************
 * Segment special region operations
 * @param  buf: buffer of any type, but should be of length BLOCK_SIZE;
 * @param  segment_addr: segment address (= seg).
 * @return length: actual length of reading / writing; -1 on error.
 * ***************************************/

/** Read inode map of the segment (covering 8 blocks, i.e. #1008 ~ #1015). */
int read_segment_imap(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int read_length = pread(file_handle, buf, IMAP_SIZE, file_offset);
    close(file_handle);
    return read_length;
}

/** Write inode map of the segment (covering 8 blocks, i.e. #1008 ~ #1015). */
int write_segment_imap(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int write_length = pwrite(file_handle, buf, IMAP_SIZE, file_offset);
    close(file_handle);
    return write_length;
}

/** Read segment summary of the segment (covering 8 blocks, i.e. #1016 ~ #1023). */
int read_segment_summary(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int read_length = pread(file_handle, buf, SUMMARY_SIZE, file_offset);
    close(file_handle);
    return read_length;
}

/** Write segment summary of the segment (covering 8 blocks, i.e. #1016 ~ #1023). */
int write_segment_summary(void* buf, int segment_addr) {
    int file_handle = open(lfs_path, O_RDWR);
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int write_length = pwrite(file_handle, buf, SUMMARY_SIZE, file_offset);
    close(file_handle);
    return write_length;
}



/** **************************************
 * Filesystem special region operations
 * @param  buf: buffer of any type, but should be of length BLOCK_SIZE;
 * @return length: actual length of reading / writing; -1 on error.
 * ***************************************/

/** Read checkpoints of LFS (covering 1 block, at #CHECKPOINT_ADDR). */
int read_checkpoints(void* buf) {
    int file_handle = open(lfs_path, O_RDWR);
    int read_length = pread(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    close(file_handle);
    return read_length;
}

/** Write checkpoints of LFS (covering 1 block, at #CHECKPOINT_ADDR). */
int write_checkpoints(void* buf) {
    int file_handle = open(lfs_path, O_RDWR);
    int write_length = pwrite(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    close(file_handle);
    return write_length;
}

/** Read superblock of LFS (covering 1 block, at #SUPERBLOCK_ADDR). */
int read_superblock(void* buf) {
    int file_handle = open(lfs_path, O_RDWR);
    int read_length = pread(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    close(file_handle);
    return read_length;
}

/** Write superblock of LFS (covering 1 block, at #SUPERBLOCK_ADDR). */
int write_superblock(void* buf) {
    int file_handle = open(lfs_path, O_RDWR);
    int write_length = pwrite(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    close(file_handle);
    return write_length;
}


/** **************************************
 * Timestamp and permission utilities.
 * ***************************************/
/** Update atime according to FUNC_ATIME_ flags */
void update_atime(struct inode &cur_inode, struct timespec &new_time) {
    if (FUNC_ATIME_REL) {
        if ((cur_inode.atime.tv_sec < cur_inode.mtime.tv_sec)
            || (cur_inode.atime.tv_sec < cur_inode.ctime.tv_sec)
            || (cur_inode.atime.tv_sec - new_time.tv_sec > FUNC_ATIME_REL_THRES))
            { cur_inode.atime = new_time;
              cur_inode.ctime = new_time; }
    } else {
        cur_inode.atime = new_time;
        cur_inode.ctime = new_time;
    }

    // Trace checkpoint updates (every CKPT_UPDATE_INTERVAL seconds).
    if (new_time.tv_sec - last_ckpt_update_time.tv_sec >= CKPT_UPDATE_INTERVAL) {
        generate_checkpoint();
        last_ckpt_update_time = new_time;
    }
}


/** **************************************
 * Public variable locks.
 * ***************************************/
void acquire_reader_lock() {
    global_lock.lock();
    return;
};

void release_reader_lock() {
    global_lock.unlock();
    return;
};

void acquire_writer_lock() {
    global_lock.lock();
    return;
};

void release_writer_lock() {
    global_lock.unlock();
    return;
};