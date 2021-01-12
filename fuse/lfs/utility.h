#pragma once

#include <sys/stat.h>   /* struct timespec */
#include <fuse.h>       /* sturct fuse_context */
#include <mutex>
#include <set>
#include <condition_variable>

// [CAUTION] to represent "empty" values, we follow the following convention:
// (1) "empty" inode = 0;
// (2) "empty" block address = -1.

/** **************************************
 * Basic physical structure.
 * ***************************************/
const int BLOCK_SIZE        = 1024;
const int SEGMENT_SIZE      = 1048576;
const int BLOCKS_IN_SEGMENT = 1024;
const int TOT_SEGMENTS      = 100;
const int TOT_INODES        = 100000;

typedef char block[BLOCK_SIZE];



/** **************************************
 * File logical structures.
 * ***************************************/

const int MAX_NUM_INODE     = 100000;
const int NUM_INODE_DIRECT  = 232;
/** Inode Block: maintaining metadata of files / directories.
 * i_number: a positive integer (0 stands for an "empty" inode).
 * mode: 1 = file, 2 = dir; use -1 to indicate indirect blocks,
 *       which are not headers and cannot be directly accessed.
 * direct: direct pointers to data blocks.
 * next_indirect: [CAUTION] inode number of the next indirect block.
 *                We use "ghost" inode numbers to facilitate efficient modification of inodes.
 */
struct inode {
    int i_number;                   // [CONST] Inode number.
    int mode;                       // [CONST] Mode of the file (file = 1, dir = 2; non-head = -1).
    int num_links;                  // [VAR] Number of hard links.
    int fsize_byte;                 // [VAR] File size (in bytes).
    int fsize_block;                // [VAR] File size (in blocks, rounded up).
    int io_block;                   // [CONST] Size of disk data transmission unit.
    int permission;                 // [VAR] Permission (lowest 9 bits, UGO x RWX)
    int perm_uid;                   // [CONST] ID of the owner.
    int perm_gid;                   // [CONST] Group ID of the owner.
    int device;                     // [CONST] Device identifier.
    struct timespec atime;          // [VAR] Last access time (2*long = 4*int).
    struct timespec mtime;          // [VAR] Last modify time (2*long = 4*int).
    struct timespec ctime;          // [VAR] Last change time (2*long = 4*int).
    int num_direct;                 // [VAR] Index of last valid entry in direct[].
    int direct[NUM_INODE_DIRECT];   // Array of direct pointers.
    int next_indirect;              // Inode number of the next indirect block.  
};
const int MODE_FILE         = 1;
const int MODE_DIR          = 2;
const int MODE_MID_INODE    = -1;


const int MAX_FILENAME_LEN  = 60;
const int MAX_DIR_ENTRIES   = 16;
struct dir_entry {
    char filename[MAX_FILENAME_LEN];// Filename (const-length C-style string, end with '\0').
    int i_number;                   // Inode number.
};
/** Directory Data Block: maintaining structure within a directory file.
 */
typedef struct dir_entry directory[MAX_DIR_ENTRIES];


/** **************************************
 * Segment logical structures.
 * ***************************************/
const int DATA_BLOCKS_IN_SEGMENT    = BLOCKS_IN_SEGMENT - 16;
const int IMAP_SIZE                 = 8 * (BLOCK_SIZE-16);
const int SUMMARY_SIZE              = 8 * (BLOCK_SIZE-16);
const int SEGMETA_SIZE              = 12;
const int IMAP_OFFSET               = SEGMENT_SIZE - 16*BLOCK_SIZE;
const int SUMMARY_OFFSET            = SEGMENT_SIZE - 16*BLOCK_SIZE + IMAP_SIZE;
const int SEGMETA_OFFSET            = SEGMENT_SIZE - 16*BLOCK_SIZE + IMAP_SIZE + SUMMARY_SIZE;

/** Inode-Map Data Block: tracing all inodes within the segment.
 * This is a dictionary, where i_number is "key" and inode_block is "value".
 * Random (and possibly non-consecutive) storation is allowed.
 * Inode maps are always read and written in chunks (there are 8 of them) for each segment.
 */
struct imap_entry {
    int i_number;           // Inode number.
    int inode_block;        // Index of the inode block.
};
typedef struct imap_entry inode_map[DATA_BLOCKS_IN_SEGMENT];

/** Segment Summary Block: tracing all blocks within the segment.
 * This is an array, so sequential storation is required.
 * Segment summaries are always read and written in chunks (there are 8 of them) for each segment.
 */
struct summary_entry {
    int i_number;           // Inode number of corresponding file.
    int direct_index;       // The index of direct[] in that inode, pointing to the block.
};
typedef struct summary_entry segment_summary[DATA_BLOCKS_IN_SEGMENT];

/** Segment Metadata Block: storing metadata of the segment.
 * Up to 256 bytes (64 int variables can be stored as metadata, although we do not use all.
 */
struct segment_metadata {
    int update_sec;         // The second part of last update time of the segment.
    int update_nsec;        // The nano-second part of last update time of the segment.
    int cur_block;          // Next available block within the segment.
};


/** **************************************
 * File-system logical structures.
 * ***************************************/

const int SUPERBLOCK_ADDR = TOT_SEGMENTS * SEGMENT_SIZE;
const int SUPERBLOCK_SIZE = 20;
/** Superblock: recording basic information (constants) about LFS.
 */
struct superblock {
    int tot_inodes;         // [CONST] Maximum number of inodes.
    int tot_blocks;         // [CONST] Maximum number of blocks.
    int tot_segments;       // [CONST] Maximum number of segments.
    int block_size;         // [CONST] Size of a block (in bytes, usu. 1024 kB).
    int segment_size;       // [CONST] Size of a segment (in bytes).
};


const int CHECKPOINT_ADDR = TOT_SEGMENTS * SEGMENT_SIZE + BLOCK_SIZE;
const int CHECKPOINT_SIZE = 2 * (28+TOT_SEGMENTS);
const int CKPT_UPDATE_INTERVAL = 30;    // Minimum interval for checkpoint update (in seconds).
/** Checkpoint Block: recording periodical checkpoints of volatile information.
 * We should assign 2 checkpoints and use them in turns (for failure restoration).
 * [CAUTION] Checkpoints are declared in utility.cpp/h, retrieved in system.cpp, and saved in blockio.cpp.
 */
struct checkpoint_entry {
    char segment_bitmap[TOT_SEGMENTS];  // Indicate whether each segment is alive.
    bool is_full;                       // Indicate whether LFS is already full.
    int count_inode;                    // Current number of inodes (monotone increasing).
    int cur_segment;                    // Next available segment.
    int cur_block;                      // Next available block (in the segment).
    int next_imap_index;                // Index of next free imap entry (within the segment).
    int timestamp_sec;                  // Timestamp of last change to this checkpoint.
    int timestamp_nsec;
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
int read_segment_summary(void* buf, int segment_addr);
int read_segment_metadata(void* buf, int segment_addr);

int read_checkpoints(void* buf);
int write_checkpoints(void* buf);

int read_superblock(void* buf);
int write_superblock(void* buf);


/** **************************************
 * Global state variables.
 * ***************************************/
extern char* lfs_path;                              // File handle should be local: only store the path.
extern char segment_buffer[SEGMENT_SIZE];
extern char segment_bitmap[TOT_SEGMENTS];
extern bool is_full;
extern int inode_table[MAX_NUM_INODE];
extern int count_inode;
extern int cur_segment, cur_block;                  // cur_block is the NEXT available block.
extern int next_checkpoint, next_imap_index;
extern struct timespec last_ckpt_update_time;       // Record the last time to update checkpoints.

extern segment_summary cached_segsum[TOT_SEGMENTS]; // In-memory segment summary.
extern inode cached_inode_array[MAX_NUM_INODE];     // In-memory inode array.

const long long FILE_SIZE = 1ll * SEGMENT_SIZE * TOT_SEGMENTS + 2 * BLOCK_SIZE;
const int ROOT_DIR_INUMBER = 1;


/** **************************************
 * Debug and error-reporting flags.
 * ***************************************/
const bool DEBUG_PRINT_COMMAND  = 0;    // Print the name of each command.
const bool DEBUG_METADATA_INODE = 0;    // Print inode for each metadata query.
const bool DEBUG_DIRECTORY      = 0;    // Print debug information in directory.cpp.
const bool DEBUG_FILE           = 0;    // Print debug information in file.cpp.
const bool DEBUG_PATH           = 0;    // Print debug information in path.cpp.
const bool DEBUG_BLOCKIO        = 0;    // Print (seg, blk) for each appended block.
const bool DEBUG_LOCATE_REPORT  = 0;    // Generate report for each locate() (in path.cpp).
const bool DEBUG_CKPT_REPORT    = 1;    // Print checkpoint after each storation.
const bool DEBUG_GARBAGE_COL    = 1;    // Print debug information for garbage collection utilities.
const bool DEBUG_GC_BLOCKIO     = 0;    // Print GC buffer block I/O information.

const bool ERROR_METADATA       = 0;    // Report errors in metadata.cpp assoc. with locate().
const bool ERROR_DIRECTORY      = 1;    // Report directory operation errors in directory.cpp.
const bool ERROR_FILE           = 1;    // Report file operation errors in file.cpp.
const bool ERROR_PATH           = 1;    // Report low-level errors in path.cpp.
const bool ERROR_PERMCPP        = 1;    // Report errors in perm.cpp.
const bool ERROR_PERM           = 1;    // Report permission errors in all source files.



/** **************************************
 * Timestamp and permission utilities.
 * ***************************************/
const bool FUNC_TIMESTAMPS      = 0;        // Enable mtime and ctime.
const bool FUNC_ATIME_DIR       = 0;        // Enable atime update for directories.
const bool FUNC_ATIME_FILE      = 0;        // Enable atime update for files.
const bool FUNC_ATIME_REL       = 1;        // Enable relative atime (as with -relatime).
const int FUNC_ATIME_REL_THRES  = 3600;     // Threshold interval for updating (relative) atime.
void update_atime(struct inode* cur_inode, struct timespec &new_time);

const bool ENABLE_PERMISSION    = 1;        // Whether to enable permission control or not.
const bool ENABLE_ACCESS_PERM   = 1;        // Whether to enable permission control in access() or not.

const int PERM_READ             = 4;        // Read permission (R_OK).
const int PERM_WRITE            = 2;        // Write permission (W_OK).
const int PERM_EXEC             = 1;        // Execute permission (X_OK)
bool verify_permission(int mode, struct inode* f_inode, struct fuse_context* u_info, bool enable);


/** **************************************
 * Public variable locks.
 * ***************************************/
extern std::mutex global_lock, io_lock;
extern std::mutex inode_lock[MAX_NUM_INODE];

extern std::mutex gc_lock;
extern std::mutex num_opt_lock;

extern std::condition_variable cond_gc;
extern std::condition_variable cond_opt;

extern int num_opt;
extern bool trigger_gc;

void acquire_segment_lock();
void release_segment_lock();
void acquire_counter_lock();
void release_counter_lock();

/* Use lock holder classes, so that on all exit paths,
 * locks are automatically released by destructors. */
class opt_lock_holder {
public:
    opt_lock_holder();
    ~opt_lock_holder();
};

class gc_lock_holder {
public:
    gc_lock_holder();
    ~gc_lock_holder();
};


/** **************************************
 * Garbage collection.
 * ***************************************/
const int CLEAN_THRESHOLD   = (int) (0.80*TOT_SEGMENTS);
const int CLEAN_THORO_THRES = (int) (0.96*TOT_SEGMENTS);
const int CLEAN_NUM         = (int) (0.3*TOT_SEGMENTS);
const int CLEAN_BELOW_UTIL  = (int) (0.01*BLOCKS_IN_SEGMENT);

const bool DO_GARBCOL_ON_START = 1;     // Force a thorough garbage collection on start of LFS.

// Control change of garbage collection level.
// Logic: when a garbage collection fails to release much space, increments level;
//        it will not repeat current-level garbage collection before a constant time interval.
extern int cur_garbcol_level;           // Current garbage collection level.
extern int last_garbcol_time;           // Last time garbage collection was triggered.

const int GARBCOL_LEVEL_80  = 1;
const int GARBCOL_LEVEL_96  = 2;
const int GARBCOL_LEVEL_100 = 3;
const int GARBCOL_INTERVAL  = 60;

const int CLEAN_NORM_FAIL   = (int) (0.65*TOT_SEGMENTS);
const int CLEAN_THORO_FAIL  = (int) (0.85*TOT_SEGMENTS);



const bool USE_CACHE        = true;