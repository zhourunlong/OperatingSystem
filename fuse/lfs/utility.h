#pragma once

/****************************************
 * Basic physical structure.
 ****************************************/
const int BLOCK_SIZE = 1024;
const int SEGMENT_SIZE = 1048576;
const int BLOCKS_IN_SEGMENT = 1024;

typedef char block[BLOCK_SIZE];


/****************************************
 * File logical structures.
 ****************************************/
/** Inode block
 */
const int MAX_NUM_INODE = 100000;
const int NUM_INODE_DIRECT = 240;
struct inode {
    int i_number;      // [CONST] Inode number.
    int mode;          // [CONST] Mode of the file (file / directory).
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
    int direct[NUM_INODE_DIRECT];  // Array of direct pointers.
    int next_indirect;             // Pointer to the next indirect block.  
};

/** Indirect block
 */
const int NUM_INDIR_DIRECT = 255;
struct indirect {
    int direct[NUM_INDIR_DIRECT];  // Array of direct pointers.
    int next_indirect;             // Pointer to the next indirect block.  
};

/** Directory data block
 */
const int MAX_FILENAME_LEN = 28;
const int MAX_DIR_ENTRIES = 32;
struct dir_entry {
    char filename[MAX_FILENAME_LEN];    // Filename (const-length C-style string, end with '\0').
    int i_number;                  // Inode number.
};
typedef struct dir_entry directory[MAX_DIR_ENTRIES];


/****************************************
 * Segment logical structures.
 ****************************************/
const int IMAP_SIZE = 8 * BLOCK_SIZE;
const int SUMMARY_SIZE = 8 * BLOCK_SIZE;
const int IMAP_OFFSET = SEGMENT_SIZE - IMAP_SIZE - SUMMARY_SIZE;
const int SUMMARY_OFFSET = SEGMENT_SIZE - IMAP_SIZE;

/** Inode-map data block
 * Random storation.
 * CAUTION: we should include 8 inode-map blocks in each segment.
 */
const int MAX_IMAP_ENTRIES = 128;
struct imap_entry {
    int i_number;          // Inode number.
    int inode_block;       // Index of the inode block.
};
typedef struct imap_entry inode_map[MAX_IMAP_ENTRIES];

/** Segment summary block
 * Sequential storation.
 * CAUTION: we should include 8 summary blocks in each segment.
 */
const int MAX_SUMMARY_ENTRIES = 128;
struct summary_entry {
    int i_number;          // Inode number of corresponding file.
    int block_offset;      // Block offset in corresponding file.
};
typedef struct summary_entry segment_summary[MAX_SUMMARY_ENTRIES];


/****************************************
 * File-system logical structures.
 ****************************************/
extern int TOT_INODES, TOT_BLOCKS, TOT_SEGMENTS;

/** Superblock
 */
struct superblock {
    int tot_inodes;    // [CONST] Maximum number of inodes.
    int tot_blocks;    // [CONST] Maximum number of blocks.
    int tot_segments;  // [CONST] Maximum number of segments.
    int block_size;    // [CONST] Size of a block (in bytes, usu. 1024 kB).
    int segment_size;  // [CONST] Size of a segment (in bytes).
};

/** Checkpoint block
 * CAUTION: we should assign 2 checkpoints and use them in turns.
 */
const int MAX_NUM_SEGMENTS = 128;
struct checkpoint_entry {
    char segment_bitmap[MAX_NUM_SEGMENTS];  // Indicate whether each segment is alive.
    int count_inode;                   // Current number of inodes (monotone increasing).
    int cur_segment;                   // Next available segment.
    int cur_block;                     // Next available block (in the segment).
    int timestamp;                     // Timestamp of last change to this checkpoint.
};
typedef struct checkpoint_entry checkpoints[2];


/****************************************
 * Functions for actual file reads / writes.
 ****************************************/
int read_block();
int read_segment_imap();
int read_segment_summary();
int write_segment();


/****************************************
 * Global state variables.
 ****************************************/
extern int file_handle;
extern char segment_buffer[SEGMENT_SIZE];
extern int inode_table[MAX_NUM_INODE];
extern int count_inode, cur_segment, cur_block;