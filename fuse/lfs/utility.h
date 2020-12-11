#pragma once

#include <sys/stat.h>  /* struct timespec */

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

const int MAX_NUM_INODE = 100000;
const int NUM_INODE_DIRECT = 232;
/** Inode Block: maintaining metadata of files / directories.
 * i_number: a positive integer (0 stands for an "empty" inode).
 * mode: 1 = file, 2 = dir; use -1 to indicate indirect blocks,
 *       which are not headers and cannot be directly accessed.
 * direct: direct pointers to data blocks.
 * next_indirect: [CAUTION] inode number of the next indirect block.
 *                We use "ghost" inode numbers to facilitate efficient modification of inodes.
 */
struct inode {
    int i_number;           // [CONST] Inode number.
    int mode;               // [CONST] Mode of the file (file = 1, dir = 2; non-head = -1).
    int num_links;          // [VAR] Number of hard links.
    int fsize_byte;         // [VAR] File size (in bytes).
    int fsize_block;        // [VAR] File size (in blocks, rounded up).
    int io_block;           // [CONST] Size of disk data transmission unit.
    int permission;         // [VAR] Permission (lowest 9 bits, UGO x RWX)
    int perm_uid;           // [CONST] ID of the owner.
    int perm_gid;           // [CONST] Group ID of the owner.
    int device;             // [CONST] Device identifier.
    struct timespec atime;  // [VAR] Last access time (2*long = 4*int).
    struct timespec mtime;  // [VAR] Last modify time (2*long = 4*int).
    struct timespec ctime;  // [VAR] Last change time (2*long = 4*int).
    int num_direct;         // [VAR] Index of last valid entry in direct[].
    int direct[NUM_INODE_DIRECT];  // Array of direct pointers.
    int next_indirect;             // Inode number of the next indirect block.  
};
const int MODE_FILE = 1;
const int MODE_DIR = 2;
const int MODE_MID_INODE = -1;


const int MAX_FILENAME_LEN = 28;
const int MAX_DIR_ENTRIES = 32;
struct dir_entry {
    char filename[MAX_FILENAME_LEN];    // Filename (const-length C-style string, end with '\0').
    int i_number;                  // Inode number.
};
/** Directory Data Block: maintaining structure within a directory file.
 */
typedef struct dir_entry directory[MAX_DIR_ENTRIES];


/** **************************************
 * Segment logical structures.
 * ***************************************/
const int IMAP_SIZE = 8 * (BLOCK_SIZE-16);
const int SUMMARY_SIZE = 8 * (BLOCK_SIZE-16);
const int IMAP_OFFSET = SEGMENT_SIZE - IMAP_SIZE - SUMMARY_SIZE;
const int SUMMARY_OFFSET = SEGMENT_SIZE - IMAP_SIZE;
const int DATA_BLOCKS_IN_SEGMENT = BLOCKS_IN_SEGMENT - 16;

/** Inode-Map Data Block: tracing all inodes within the segment.
 * This is a dictionary, where i_number is "key" and inode_block is "value".
 * Random (and possibly non-consecutive) storation is allowed.
 * Inode maps are always read and written in chunks (there are 8 of them) for each segment.
 */
struct imap_entry {
    int i_number;          // Inode number.
    int inode_block;       // Index of the inode block.
};
typedef struct imap_entry inode_map[DATA_BLOCKS_IN_SEGMENT];

/** Segment Summary Block: tracing all blocks within the segment.
 * This is an array, so sequential storation is required.
 * Segment summaries are always read and written in chunks (there are 8 of them) for each segment.
 */
struct summary_entry {
    int i_number;          // Inode number of corresponding file.
    int direct_index;      // The index of direct[] in that inode, pointing to the block.
};
typedef struct summary_entry segment_summary[DATA_BLOCKS_IN_SEGMENT];


/** **************************************
 * File-system logical structures.
 * ***************************************/

const int SUPERBLOCK_ADDR = TOT_SEGMENTS * SEGMENT_SIZE;
const int SUPERBLOCK_SIZE = 20;
/** Superblock: recording basic information (constants) about LFS.
 */
struct superblock {
    int tot_inodes;    // [CONST] Maximum number of inodes.
    int tot_blocks;    // [CONST] Maximum number of blocks.
    int tot_segments;  // [CONST] Maximum number of segments.
    int block_size;    // [CONST] Size of a block (in bytes, usu. 1024 kB).
    int segment_size;  // [CONST] Size of a segment (in bytes).
};


const int CHECKPOINT_ADDR = TOT_SEGMENTS * SEGMENT_SIZE + BLOCK_SIZE;
const int CHECKPOINT_SIZE = 2 * (24+TOT_SEGMENTS);
/** Checkpoint Block: recording periodical checkpoints of volatile information.
 * We should assign 2 checkpoints and use them in turns (for failure restoration).
 */
struct checkpoint_entry {
    char segment_bitmap[TOT_SEGMENTS]; // Indicate whether each segment is alive.
    int count_inode;                   // Current number of inodes (monotone increasing).
    int cur_segment;                   // Next available segment.
    int cur_block;                     // Next available block (in the segment).
    int next_imap_index;               // Index of next free imap entry (within the segment).
    int timestamp;                     // Timestamp of last change to this checkpoint.
};
typedef struct checkpoint_entry checkpoints[2];


/** **************************************
 * Pretty-print functions.
 * ***************************************/
void print(struct inode* node);
void print(directory dir);
void print(inode_map imap);
void print(segment_summary segsum);
void print(struct superblock* sblk);
void print(checkpoints ckpt);

const int DISP_BIT_BIN = 1;
const int DISP_BYTE_DEC = 2;
const int DISP_BYTE_HEX = 3;
const int DISP_WORD_DEC = 4;
const int DISP_WORD_HEX = 5;
void print(block blk, int disp);

void print_inode_table();

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
extern int next_checkpoint, next_imap_index;

const int FILE_SIZE = SEGMENT_SIZE * TOT_SEGMENTS + 2 * BLOCK_SIZE;
const int ROOT_DIR_INUMBER = 1;


/** **************************************
 * Debug and error-reporting flags.
 * ***************************************/
const bool DEBUG_PRINT_COMMAND  = true;     // Print the name of each command.
const bool DEBUG_METADATA_INODE = false;    // Print inode for each metadata query.
const bool DEBUG_DIRECTORY      = true;     // Print debug information in directory.cpp.
const bool DEBUG_FILE           = true;     // Print debug information in file.cpp.
const bool DEBUG_PATH           = true;     // Print debug information in path.cpp.
const bool DEBUG_LOCATE_REPORT  = false;    // Generate report for each locate() (in path.cpp).

const bool ERROR_METADATA       = false;    // Report errors in metadata.cpp assoc. with locate().
const bool ERROR_DIRECTORY      = true;     // Report directory operation errors in directory.cpp.
const bool ERROR_FILE           = true;     // Report file operation errors in file.cpp.
const bool ERROR_PATH           = true;     // Report low-level errors in path.cpp.
const bool ERROR_PERM           = true;     // Report permission operation errors in perm.cpp.


/** **************************************
 * Functionality flags.
 * ***************************************/
const bool FUNC_ATIME_DIR       = false;    // Enable atime update for directories.
const bool FUNC_ATIME_REL       = false;    // Enable relative atime (as with -relatime).
const int FUNC_ATIME_REL_THRES  = 3600;     // Threshold interval for updating (relative) atime.
void update_atime(struct inode &cur_inode, struct timespec &new_time);
