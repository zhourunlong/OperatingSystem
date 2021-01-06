#include "system.h"

#include "logger.h"
#include "print.h"
#include "utility.h"
#include "blockio.h"
#include "path.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

extern char* current_working_dir;

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;

    /* Retrive system time (for atime updates). */
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    last_ckpt_update_time = cur_time;


    /* ****************************************
     * Create / open LFS from disk.
     * ****************************************/
    std::string _lfs_path = current_working_dir;
    _lfs_path += "lfs.data";
    lfs_path = (char*) malloc(_lfs_path.length() + 2);
    strcpy(lfs_path, _lfs_path.c_str());

    if (access(lfs_path, R_OK) != 0) {    // Disk file does not exist.
        logger(DEBUG, "[INFO] Disk file (lfs.data) does not exist. Try to create it and initialize to 0.\n");
        
        initialize_disk_file();
        logger(DEBUG, "[INFO] Successfully initialized the file system.\n");
    } else {
        // Try to open an existing file.
        int file_handle = open(lfs_path, O_RDWR);
        if (file_handle <= 0) {
            logger(ERROR, "[FATAL ERROR] Fail to open existing disk file (lfs.data). Please contact administrator.\n");
            exit(-1);
        }
        close(file_handle);
        logger(DEBUG, "[INFO] Successfully found an existing file system.\n");

        load_from_disk_file();
        logger(DEBUG, "[INFO] Successfully loaded an existing file system.\n");
    }

	return NULL;
}


void o_destroy(void* private_data) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "DESTROY, %p\n", private_data);
    
    // Save LFS to disk.
    add_segbuf_metadata();
    write_segment(segment_buffer, cur_segment);
    segment_bitmap[cur_segment] = 1;
    generate_checkpoint();

    /* ========== Debug ==========
    print_inode_table();
    //collect_garbage(true, true);
    //generate_checkpoint();
    //print_inode_table();

    // Initialize inode table in memory (by simulation).
    int _inode_table[MAX_NUM_INODE];
    memset(_inode_table, -1, sizeof(_inode_table));

    // Traverse all segments to reconstruct in-memory inode sturcture.
    int inode_update_sec[MAX_NUM_INODE];
    int inode_update_nsec[MAX_NUM_INODE];
    memset(inode_update_sec, 0, sizeof(inode_update_sec));
    memset(inode_update_nsec, 0, sizeof(inode_update_nsec));

    inode_map imap;
    imap_entry im_entry;
    int newest_seg = -1, newest_sec = 0, newest_nsec = 0, newest_block = 0, newest_imap_index = 0;
    for (int seg=0; seg<TOT_SEGMENTS; seg++) {
        segment_summary seg_sum;
        read_segment_summary(&seg_sum, seg);
        memcpy(&cached_segsum[seg], &seg_sum, sizeof(seg_sum));

        struct segment_metadata seg_metadata;
        read_segment_metadata(&seg_metadata, seg);
        int seg_sec   = seg_metadata.update_sec;
        int seg_nsec  = seg_metadata.update_nsec;
        int seg_block = seg_metadata.cur_block;
        int seg_imap_index;

        read_segment_imap(imap, seg);
        for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++) {
            im_entry = imap[i];
            if (im_entry.i_number > 0) {
                int inode_sec  = inode_update_sec[im_entry.i_number];
                int inode_nsec = inode_update_nsec[im_entry.i_number];
                if ((inode_sec <= seg_sec) || ((inode_sec == seg_sec) && (inode_nsec <= seg_nsec))) {
                    _inode_table[im_entry.i_number] = im_entry.inode_block;
                    inode_update_sec[im_entry.i_number]  = seg_sec;
                    inode_update_nsec[im_entry.i_number] = seg_nsec;
                    if (im_entry.i_number > count_inode)
                        count_inode = im_entry.i_number;
                }
            } else {
                seg_imap_index = i;
                break;
            }
        }

        if ((newest_sec < seg_sec) || ((newest_sec == seg_sec) && (newest_nsec < seg_nsec))) {
            newest_seg = seg;
            newest_sec = seg_sec;
            newest_nsec = seg_nsec;
            newest_block = seg_block;
            newest_imap_index = seg_imap_index;
        }
    }

    struct inode inode_block;
    for (int i=1; i<=count_inode; i++) {
        if (_inode_table[i] >= 0) {
            get_block(&inode_block, _inode_table[i]);
            printf("Inode #%d / #%d at block #%d.\n", i, inode_block.i_number, _inode_table[i]);
        }
    }
    ========== Debug========== */
    
    logger(DEBUG, "[INFO] Successfully saved current state to disk and exited.\n");
}



void initialize_disk_file() {
    // Create a file.
    int file_handle = open(lfs_path, O_RDWR | O_CREAT, 0777);
    if (file_handle <= 0) {
        logger(ERROR, "[FATAL ERROR] Fail to create a new disk file (lfs.data).\n");
        exit(-1);
    }

    // Fill 0 into the file.
    char* buf = (char*) malloc(FILE_SIZE);
    memset(buf, 0, FILE_SIZE);
    pwrite(file_handle, buf, FILE_SIZE, 0);
    free(buf);

    // Must close file after formatting.
    close(file_handle); 
    logger(DEBUG, "[INFO] Successfully created a new disk file (lfs.data).\n");


    // Initialize global state variables.
    memset(segment_bitmap, 0, sizeof(segment_bitmap));
    is_full         = false;
    count_inode     = 0;
    cur_segment     = 0;
    cur_block       = 0;
    next_checkpoint = 0;
    next_imap_index = 0;
    memset(segment_buffer, 0, sizeof(segment_buffer));
    memset(inode_table, -1, sizeof(inode_table));

    // Initialize superblock.
    struct superblock init_sblock = {
        tot_inodes   : TOT_INODES,
        tot_blocks   : BLOCKS_IN_SEGMENT * TOT_SEGMENTS,
        tot_segments : TOT_SEGMENTS,
        block_size   : BLOCK_SIZE,
        segment_size : SEGMENT_SIZE
    };
    write_superblock(&init_sblock);
    print(&init_sblock);

    // Initialize root directory (i_number = 1).
    struct inode* root_inode;
    file_initialize(root_inode, MODE_DIR, 0777);

    buf = (char*) malloc(BLOCK_SIZE);
    memset(buf, 0, BLOCK_SIZE);
    file_add_data(root_inode, buf);
    free(buf);

    new_inode_block(root_inode);
    write_segment(segment_buffer, 0);
    segment_bitmap[0] = 1;


    // Generate the first checkpoint.
    generate_checkpoint();
}


void load_from_disk_file() {
    /* (A) Read the (newer) checkpoint. */
    checkpoints ckpt;
    read_checkpoints(&ckpt);
    print(ckpt);

    int latest_index = 0;
    if (ckpt[0].timestamp < ckpt[1].timestamp)
        latest_index = 1;
    next_checkpoint = 1 - latest_index;
    
    memcpy(segment_bitmap, ckpt[latest_index].segment_bitmap, sizeof(segment_bitmap));
    is_full         = ckpt[latest_index].is_full;
    count_inode     = ckpt[latest_index].count_inode;
    cur_segment     = ckpt[latest_index].cur_segment;
    cur_block       = ckpt[latest_index].cur_block;
    next_imap_index = ckpt[latest_index].next_imap_index;
    if ((count_inode <= 0) || (cur_segment < 0) || (cur_segment >= TOT_SEGMENTS) \
                           || (cur_block < 0) || (cur_block >= DATA_BLOCKS_IN_SEGMENT)) {
        logger(ERROR, "[FATAL ERROR] Corrupt file system on disk: invalid checkpoint entry.\n");
        exit(-1);
    }
    

    /* (B) Restore the inode table in memory. */
    // (1) Initialize inode table in memory (by simulation).
    // Note: since #0 is a valid block number, we should initialized to -1.
    memset(inode_table, -1, sizeof(inode_table));

    // (2) Traverse all segments to reconstruct latest inode table in memory.
    int inode_update_sec[MAX_NUM_INODE];
    int inode_update_nsec[MAX_NUM_INODE];
    memset(inode_update_sec, 0, sizeof(inode_update_sec));
    memset(inode_update_nsec, 0, sizeof(inode_update_nsec));

    inode_map imap;
    imap_entry im_entry;
    int newest_seg = -1, newest_sec = 0, newest_nsec = 0, newest_block = 0, newest_imap_index = 0;
    for (int seg=0; seg<TOT_SEGMENTS; seg++) {
        segment_summary seg_sum;
        read_segment_summary(&seg_sum, seg);
        memcpy(&cached_segsum[seg], &seg_sum, sizeof(seg_sum));

        struct segment_metadata seg_metadata;
        read_segment_metadata(&seg_metadata, seg);
        int seg_sec   = seg_metadata.update_sec;
        int seg_nsec  = seg_metadata.update_nsec;
        int seg_block = seg_metadata.cur_block;
        int seg_imap_index;

        read_segment_imap(imap, seg);
        for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++) {
            im_entry = imap[i];
            if (im_entry.i_number > 0) {
                int inode_sec  = inode_update_sec[im_entry.i_number];
                int inode_nsec = inode_update_nsec[im_entry.i_number];
                if ((inode_sec <= seg_sec) || ((inode_sec == seg_sec) && (inode_nsec <= seg_nsec))) {
                    inode_table[im_entry.i_number] = im_entry.inode_block;
                    inode_update_sec[im_entry.i_number]  = seg_sec;
                    inode_update_nsec[im_entry.i_number] = seg_nsec;
                    if (im_entry.i_number > count_inode)
                        count_inode = im_entry.i_number;
                }
            } else {
                seg_imap_index = i;
                break;
            }
        }

        if ((newest_sec < seg_sec) || ((newest_sec == seg_sec) && (newest_nsec < seg_nsec))) {
            newest_seg          = seg;
            newest_block        = seg_block;
            newest_imap_index   = seg_imap_index;
            newest_sec          = seg_sec;
            newest_nsec         = seg_nsec;
        }
    }


    /* (C) Restore segment buffer. */
    // Restore block pointers (i.e., cur_segment and cur_block).
    if ((newest_block == DATA_BLOCKS_IN_SEGMENT-1) || (newest_imap_index == DATA_BLOCKS_IN_SEGMENT)) {
        cur_segment = newest_seg;
        get_next_free_segment();
    } else {
        cur_segment     = newest_seg;
        cur_block       = newest_block;
        next_imap_index = newest_imap_index;
    }
    
    // Initialize segment buffer in memory by reading current segment.
    read_segment(segment_buffer, cur_segment);


    /* (D) Reconstruct inode array in memory. */
    // Note that the inode table and segment buffer is already up-to-date.
    struct inode inode_block;
    for (int i=1; i<=count_inode; i++) {
        if (inode_table[i] >= 0) {
            get_block(&inode_block, inode_table[i]);
            cached_inode_array[i] = inode_block;
        }
    }


    /* (E) (optional) Do a thorough garbage collection for better performance. */
    if (DO_GARBCOL_ON_START) {
        collect_garbage(true);

        // Determine whether the file system is indeed full.
        int recount_full_segment = 0;
        for (int i=0; i<TOT_SEGMENTS; i++)
            recount_full_segment += segment_bitmap[i];
        if ((recount_full_segment == TOT_SEGMENTS-1) && (cur_block >= BLOCKS_IN_SEGMENT / 2)) {
            is_full = true;
            logger(WARN, "[WARNING] The file system is already full: please assign a larger disk size.\n");
        }
    } else {
        if (is_full)
            logger(WARN, "[WARNING] The file system is reportedly full: please assign a larger disk size.\n");
    }
        
    
    /* (F) Generate a checkpoint for easier recovery. */
    generate_checkpoint();
}