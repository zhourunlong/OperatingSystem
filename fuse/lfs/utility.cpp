#include "unistd.h"

#include "utility.h"
#include <stdio.h>

int file_handle;
char segment_buffer[SEGMENT_SIZE];
int inode_table[MAX_NUM_INODE];
int count_inode, cur_segment, cur_block;
int root_dir_inumber;


/** read_block
 * Read data in blocks.
 */
int read_block(void* buf, int block_addr) {
    int file_offset = block_addr * BLOCK_SIZE;
    int read_length = pread(file_handle, buf, BLOCK_SIZE, file_offset);
    return read_length;
}

/** write_block
 * Write data in blocks (not recommended).
 */
int write_block(void* buf, int block_addr) {
    int file_offset = block_addr * BLOCK_SIZE;
    int write_length = pwrite(file_handle, buf, BLOCK_SIZE, file_offset);
    return write_length;
}

/** write_segment
 * Write in segments.
 */
int write_segment(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE;
    int write_length = pwrite(file_handle, buf, SEGMENT_SIZE, file_offset);
    return write_length;
}



/** read_segment_imap
 * Read imap (8 blocks, #1008~#1015).
 */
int read_segment_imap(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int read_length = pread(file_handle, buf, IMAP_SIZE, file_offset);
    return read_length;
}

/** write_segment_imap
 * Write imap (8 blocks, #1008~#1015).
 */
int write_segment_imap(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int write_length = pwrite(file_handle, buf, IMAP_SIZE, file_offset);
    return write_length;
}


/** read_segment_summary
 * Read segment summary (8 blocks, #1016~#1023).
 */
int read_segment_summary(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int read_length = pread(file_handle, buf, SUMMARY_SIZE, file_offset);
    return read_length;
}

/** write segment_summary
 * Write segment summary (8 blocks, #1016~#1023).
 */
int write_segment_summary(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int write_length = pwrite(file_handle, buf, SUMMARY_SIZE, file_offset);
    return write_length;
}


/** read_checkpoints
 * Read checkpoints (1 block, #CHECKPOINT_ADDR).
 */
int read_checkpoints(void* buf) {
    int read_length = pread(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    return read_length;
}

/** write_checkpoints
 * Write checkpoints (1 block, #CHECKPOINT_ADDR).
 */
int write_checkpoints(void* buf) {
    int write_length = pwrite(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    return write_length;
}


/** read_superblock
 * Read superblock (1 block, #SUPERBLOCK_ADDR).
 */
int read_superblock(void* buf) {
    int read_length = pread(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    return read_length;
}

/** write_superblock
 * Write superblock (1 block, #SUPERBLOCK_ADDR).
 */
int write_superblock(void* buf) {
    int write_length = pwrite(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    return write_length;
}
