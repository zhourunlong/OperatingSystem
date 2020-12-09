#include "utility.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>

int file_handle;
char segment_buffer[SEGMENT_SIZE];
char segment_bitmap[TOT_SEGMENTS];
int inode_table[MAX_NUM_INODE];
int count_inode, cur_segment, cur_block;
int next_checkpoint, next_imap_index;


/** **************************************
 * Block operations
 * @param  buf: buffer of any type, but should be of length BLOCK_SIZE;
 * @param  block_addr: block address (= seg * BLOCKS_IN_SEGMENT + blk).
 * @param  segment_addr: segment address (= seg).
 * @return length: actual length of reading / writing; -1 on error.
 * ****************************************/

/** Read a block into the buffer. */
int read_block(void* buf, int block_addr) {
    int file_offset = block_addr * BLOCK_SIZE;
    int read_length = pread(file_handle, buf, BLOCK_SIZE, file_offset);
    return read_length;
}

/** Write a block into disk file (not recommended). */
int write_block(void* buf, int block_addr) {
    int file_offset = block_addr * BLOCK_SIZE;
    int write_length = pwrite(file_handle, buf, BLOCK_SIZE, file_offset);
    return write_length;
}

/** Read a segment into the buffer (not usual). */
int read_segment(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE;
    int read_length = pread(file_handle, buf, SEGMENT_SIZE, file_offset);
    return read_length;
}

/** Write a segment into disk file. */
int write_segment(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE;
    int write_length = pwrite(file_handle, buf, SEGMENT_SIZE, file_offset);
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
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int read_length = pread(file_handle, buf, IMAP_SIZE, file_offset);
    return read_length;
}

/** Write inode map of the segment (covering 8 blocks, i.e. #1008 ~ #1015). */
int write_segment_imap(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + IMAP_OFFSET;
    int write_length = pwrite(file_handle, buf, IMAP_SIZE, file_offset);
    return write_length;
}

/** Read segment summary of the segment (covering 8 blocks, i.e. #1016 ~ #1023). */
int read_segment_summary(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int read_length = pread(file_handle, buf, SUMMARY_SIZE, file_offset);
    return read_length;
}

/** Write segment summary of the segment (covering 8 blocks, i.e. #1016 ~ #1023). */
int write_segment_summary(void* buf, int segment_addr) {
    int file_offset = segment_addr * SEGMENT_SIZE + SUMMARY_OFFSET;
    int write_length = pwrite(file_handle, buf, SUMMARY_SIZE, file_offset);
    return write_length;
}



/** **************************************
 * Filesystem special region operations
 * @param  buf: buffer of any type, but should be of length BLOCK_SIZE;
 * @return length: actual length of reading / writing; -1 on error.
 * ***************************************/

/** Read checkpoints of LFS (covering 1 block, at #CHECKPOINT_ADDR). */
int read_checkpoints(void* buf) {
    int read_length = pread(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    return read_length;
}

/** Write checkpoints of LFS (covering 1 block, at #CHECKPOINT_ADDR). */
int write_checkpoints(void* buf) {
    int write_length = pwrite(file_handle, buf, CHECKPOINT_SIZE, CHECKPOINT_ADDR);
    return write_length;
}

/** Read superblock of LFS (covering 1 block, at #SUPERBLOCK_ADDR). */
int read_superblock(void* buf) {
    int read_length = pread(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    return read_length;
}

/** Write superblock of LFS (covering 1 block, at #SUPERBLOCK_ADDR). */
int write_superblock(void* buf) {
    int write_length = pwrite(file_handle, buf, SUPERBLOCK_SIZE, SUPERBLOCK_ADDR);
    return write_length;
}



/** **************************************
 * Pretty-print functions.
 * ***************************************/
void print(struct inode* node){
    logger(DEBUG, "[DEBUG] ******************** PRINT INODE ********************\n");
    logger(DEBUG, "ITEM  \tCONTENT     \n");
    logger(DEBUG, "======\t============\n");
    logger(DEBUG, "I_NUM\t%d\n", node->i_number);
    switch (node->mode) {
        case (MODE_FILE):
            logger(DEBUG, "MODE\tfile (1)\n");
            break;
        case (MODE_DIR):
            logger(DEBUG, "MODE\tdirectory (2)\n");
            break;
        case (MODE_MID_INODE):
            logger(DEBUG, "MODE\tnon-head inode (-1)\n");
            break;
        default:
            logger(DEBUG, "MODE\tunknown (%d)\n", node->mode);
            break;
    }
    logger(DEBUG, "N_LINK\t%d\n", node->num_links);
    logger(DEBUG, "SIZE\t%d B = %d blocks (IO = %d blocks)\n", node->fsize_byte, node->fsize_block, node->io_block);
    logger(DEBUG, "PERM\t%o (uid = %d, gid = %d)\n", node->permission, node->perm_uid, node->perm_gid);
    logger(DEBUG, "DEVICE\t%d\n", node->device);
    logger(DEBUG, "TIME\tatime = %d.%d\n\tmtime = %d.%d\n\tctime = %d.%d\n",\
                  node->atime.tv_sec, node->atime.tv_nsec,\
                  node->mtime.tv_sec, node->mtime.tv_nsec,\
                  node->ctime.tv_sec, node->ctime.tv_nsec);

    logger(DEBUG, "\ndirect[] (num = %d): ", node->num_direct);
    for (int i=0; i<node->num_direct; i++)
        logger(DEBUG, "%d ", node->direct[i]);
    logger(DEBUG, ".\n");
    logger(DEBUG, "next_indirect = %d.\n", node->next_indirect);
    logger(DEBUG, "        ==================== PRINT INODE ====================\n");
}

void print(directory dir) {
    logger(DEBUG, "[DEBUG] ******************** PRINT DIRECTORY ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tNAME        \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<MAX_DIR_ENTRIES; i++)
        if (dir[i].i_number > 0) {
            logger(DEBUG, "%d\t%d\t\t%s\n", i, dir[i].i_number, dir[i].filename);
            count++;
        }
    logger(DEBUG, "\nThere are %d files / sub-directories in this directory.\n", count);
    logger(DEBUG, "        ==================== PRINT DIRECTORY ====================\n");
}

void print(inode_map imap) {
    logger(DEBUG, "[DEBUG] ******************** PRINT INODE MAP ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tBLOCK_ADDR  \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<BLOCKS_IN_SEGMENT; i++)
        if ((imap[i].i_number > 0) && (imap[i].inode_block >= 0)) {
            logger(DEBUG, "%d\t%d\t\t%s\n", i, imap[i].i_number, imap[i].inode_block);
            count++;
        }
    logger(DEBUG, "\nThere are %d inodes in this segment.\n", count);
    logger(DEBUG, "        ==================== PRINT INODE MAP ====================\n");
}

void print(segment_summary segsum) {
    logger(DEBUG, "[DEBUG] ******************** PRINT SEGMENT SUMMARY ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tDIRECT[?]   \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<BLOCKS_IN_SEGMENT; i++)
        if (segsum[i].i_number > 0) {
            logger(DEBUG, "%d\t%d\t\t%s\n", i, segsum[i].i_number, segsum[i].direct_index);
            count++;
        }
    logger(DEBUG, "\nThere are %d non-empty blocks in this segment.\n", count);
    logger(DEBUG, "        ==================== PRINT SEGMENT SUMMARY ====================\n");
}

void print(struct superblock* sblk) {
    logger(DEBUG, "[DEBUG] ******************** PRINT SUPERBLOCK ********************\n");
    logger(DEBUG, "ITEM      \tCONTENT     \n");
    logger(DEBUG, "==========\t============\n");
    logger(DEBUG, "TOT_INODES  \t%d\n", sblk->tot_inodes);
    logger(DEBUG, "TOT_BLOCKS  \t%d\n", sblk->tot_blocks);
    logger(DEBUG, "TOT_SEGMENTS\t%d\n", sblk->tot_segments);
    logger(DEBUG, "BLOCK_SIZE  \t%d\n", sblk->block_size);
    logger(DEBUG, "SEGMENT_SIZE\t%d\n", sblk->segment_size);
    logger(DEBUG, "        ==================== PRINT SUPERBLOCK ====================\n");
}

void print(checkpoints ckpt){
    logger(DEBUG, "[DEBUG] ******************** PRINT CHECKPOINTS ********************\n");
    logger(DEBUG, "ITEM       \tCKPT[0]     \tCKPT[1]     \n");
    logger(DEBUG, "========== \t============\t============\n");

    logger(DEBUG, "SEG_BITMAP\n");
    int i = 0;
    while (i < TOT_SEGMENTS) {
        logger(DEBUG, "           \t");
        for (int j=0; j<10; j++)
            logger(DEBUG, "%d", ckpt[0].segment_bitmap[i+j]);
        logger(DEBUG, "  \t");
        for (int j=0; j<10; j++)
            logger(DEBUG, "%d", ckpt[1].segment_bitmap[i+j]);
        logger(DEBUG, "\n");
        i += 10;
    }
    
    logger(DEBUG, "COUNT_INODE\t%d   \t\t\t%d\n", ckpt[0].count_inode, ckpt[1].count_inode);
    logger(DEBUG, "CUR_SEGMENT\t%d   \t\t\t%d\n", ckpt[0].cur_segment, ckpt[1].cur_segment);
    logger(DEBUG, "CUR_BLOCK  \t%d   \t\t\t%d\n", ckpt[0].cur_block, ckpt[1].cur_block);
    logger(DEBUG, "NXT_IMAP_ID\t%d   \t\t\t%d\n", ckpt[0].next_imap_index, ckpt[1].next_imap_index);
    logger(DEBUG, "TIMESTAMP  \t%d\t%d\n", ckpt[0].timestamp, ckpt[1].timestamp);
    logger(DEBUG, "        ==================== PRINT CHECKPOINTS ====================\n");
}