#pragma once

// [CAUTION] to represent "empty" values, we follow the following convention:
// (1) "empty" inode = 0;
// (2) "empty" block address = -1.

/** **************************************
 * Basic physical structure.
 * ***************************************/
const int BLOCK_SIZE = 1024;
const int SEGMENT_SIZE = 1048576;
const int BLOCKS_IN_SEGMENT = 1024;
const int TOT_SEGMENTS = 100;
const int TOT_INODES = 100000;

typedef char block[BLOCK_SIZE];



/** **************************************
 * File logical structures.
 * ***************************************/

/** Inode Block: maintaining metadata of files / directories.
 * i_number: a positive integer (0 stands for an "empty" inode).
 * mode: 1 = file, 2 = dir; use -1 to indicate indirect blocks,
 *       which are not headers and cannot be directly accessed.
 * direct: direct pointers to data blocks.
 * next_indirect: [CAUTION] inode number of the next indirect block.
 *                We use "ghost" inode numbers to facilitate efficient modification of inodes.
 */
const int MAX_NUM_INODE = 100000;
const int NUM_INODE_DIRECT = 241;
struct inode {
    int i_number;      // [CONST] Inode number.
    int mode;          // [CONST] Mode of the file (file = 1, dir = 2; non-head = -1).
    int num_links;     // [VAR] Number of hard links.
    int fsize_byte;    // [VAR] File size (in bytes).
    int fsize_block;   // [VAR] File size (in blocks, rounded up).
    int io_block;      // [CONST] Size of disk data transmission unit.
    int permission;    // [VAR] Permission (lowest 9 bits, UGO x RWX)
    int perm_uid;      // [CONST] ID of the owner.
    int perm_gid;      // [CONST] Group ID of the owner.
    int device;        // [CONST] Device identifier.
    int atime;         // [VAR] Last access time.
    int mtime;         // [VAR] Last modify time.
    int ctime;         // [VAR] Last change time.
    int num_direct;    // [VAR] Index of last valid entry in direct[].
    int direct[NUM_INODE_DIRECT];  // Array of direct pointers.
    int next_indirect;             // Inode number of the next indirect block.  
};


/** Directory Data Block: maintaining structure within a directory file.
 */
const int MAX_FILENAME_LEN = 28;
const int MAX_DIR_ENTRIES = 32;
struct dir_entry {
    char filename[MAX_FILENAME_LEN];    // Filename (const-length C-style string, end with '\0').
    int i_number;                  // Inode number.
};
typedef struct dir_entry directory[MAX_DIR_ENTRIES];


/** **************************************
 * Segment logical structures.
 * ***************************************/
const int IMAP_SIZE = 8 * BLOCK_SIZE;
const int SUMMARY_SIZE = 8 * BLOCK_SIZE;
const int IMAP_OFFSET = SEGMENT_SIZE - IMAP_SIZE - SUMMARY_SIZE;
const int SUMMARY_OFFSET = SEGMENT_SIZE - IMAP_SIZE;

/** Inode-Map Data Block: tracing all inodes within the segment.
 * This is a dictionary, where i_number is "key" and inode_block is "value".
 * Random (and possibly non-consecutive) storation is allowed.
 * Inode maps are always read and written in chunks (there are 8 of them) for each segment.
 */
struct imap_entry {
    int i_number;          // Inode number.
    int inode_block;       // Index of the inode block.
};
typedef struct imap_entry inode_map[BLOCKS_IN_SEGMENT];

/** Segment Summary Block: tracing all blocks within the segment.
 * This is a vector, so sequential storation is required.
 * Segment summaries are always read and written in chunks (there are 8 of them) for each segment.
 */
struct summary_entry {
    int i_number;          // Inode number of corresponding file.
    int direct_index;      // The index of direct[] in that inode, pointing to the block.
};
typedef struct summary_entry segment_summary[BLOCKS_IN_SEGMENT];


/** **************************************
 * File-system logical structures.
 * ***************************************/
/** Superblock: recording basic information (constants) about LFS.
 */
const int SUPERBLOCK_ADDR = TOT_SEGMENTS * SEGMENT_SIZE;
const int SUPERBLOCK_SIZE = 20;
struct superblock {
    int tot_inodes;    // [CONST] Maximum number of inodes.
    int tot_blocks;    // [CONST] Maximum number of blocks.
    int tot_segments;  // [CONST] Maximum number of segments.
    int block_size;    // [CONST] Size of a block (in bytes, usu. 1024 kB).
    int segment_size;  // [CONST] Size of a segment (in bytes).
};

/** Checkpoint Block: recording periodical checkpoints of volatile information.
 * We should assign 2 checkpoints and use them in turns (for failure restoration).
 */
const int CHECKPOINT_ADDR = TOT_SEGMENTS * SEGMENT_SIZE + BLOCK_SIZE;
const int CHECKPOINT_SIZE = 2 * (24+TOT_SEGMENTS);
struct checkpoint_entry {
    char segment_bitmap[TOT_SEGMENTS]; // Indicate whether each segment is alive.
    int count_inode;                   // Current number of inodes (monotone increasing).
    int cur_segment;                   // Next available segment.
    int cur_block;                     // Next available block (in the segment).
    int root_dir_inumber;              // Current inode number of root directory.
    int next_imap_index;               // Index of next free imap entry (within the segment).
    int timestamp;                     // Timestamp of last change to this checkpoint.
};
typedef struct checkpoint_entry checkpoints[2];


/** **************************************
 * Functions for actual file reads / writes.
 * ***************************************/
int read_block(void* buf, int block_addr);
int write_block(void* buf, int block_addr);

int read_segment(void* buf, int segment_addr);
int write_segment(void* buf, int segment_addr);

int read_segment_imap(void* buf, int segment_addr);
int write_segment_imap(void* buf, int segment_addr);

int read_segment_summary(void* buf, int segment_addr);
int write_segment_summary(void* buf, int segment_addr);

int read_checkpoints(void* buf);
int write_checkpoints(void* buf);

int read_superblock(void* buf);
int write_superblock(void* buf);


/** **************************************
 * Global state variables.
 * ***************************************/
extern int file_handle;
extern char segment_buffer[SEGMENT_SIZE];
extern char segment_bitmap[TOT_SEGMENTS];
extern int inode_table[MAX_NUM_INODE];
extern int count_inode, cur_segment, cur_block;  // cur_block is the first available block.
extern int root_dir_inumber, next_checkpoint, next_imap_index;

const int FILE_SIZE = SEGMENT_SIZE * TOT_SEGMENTS + IMAP_SIZE + SUMMARY_SIZE;