#include "unistd.h"
#include "utility.h"

int file_handle;
char segment_buffer[SEGMENT_SIZE];
int inode_table[MAX_NUM_INODE];
int count_inode, cur_segment, cur_block;

/** read_block
 * Read data in blocks.
 */
int read_block(char* buf, int block_addr) {
    int file_offset = block_addr * BLOCK_SIZE;
    int read_length = pread(file_handle, buf, BLOCK_SIZE, file_offset);
    return read_length;
}

/** read_segment_imap
 * Read imap (8 blocks, #1008~#1015).
 */
int read_segment_imap(char* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int read_length = pread(file_handle, buf, IMAP_SIZE, file_offset);
    return read_length;
}

/** read_segment_summary
 * Read segment summary (8 blocks, #1016~#1023).
 */
int read_segment_summary(char* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int read_length = pread(file_handle, buf, SUMMARY_SIZE, file_offset);
    return read_length;
}

/** write_segment
 * Write in segments.
 */
int write_segment(char* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE;
    int write_length = pread(file_handle, buf, SEGMENT_SIZE, file_offset);
    return write_length;
}