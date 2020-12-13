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
        logger(DEBUG, "[INFO] Disk file (lfs.data) does not exist. Try to create and initialize to 0.\n");
        
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
        head_segment    = 0;
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
        struct inode* root_inode = (struct inode*) malloc(sizeof(struct inode));
        file_initialize(root_inode, MODE_DIR, 0777);

        buf = (char*) malloc(BLOCK_SIZE);
        memset(buf, 0, BLOCK_SIZE);
        file_add_data(root_inode, buf);
        free(buf);
        
        new_inode_block(root_inode);
        write_segment(segment_buffer, 0);
        segment_bitmap[0] = 1;

        // Generate first checkpoint.
        generate_checkpoint();
        checkpoints ckpt;
        read_checkpoints(&ckpt);
        print(ckpt);

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


        // Read the (newer) checkpoint.
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
        head_segment    = ckpt[latest_index].head_segment;
        cur_segment     = ckpt[latest_index].cur_segment;
        cur_block       = ckpt[latest_index].cur_block;
        next_imap_index = ckpt[latest_index].next_imap_index;
        if ((count_inode <= 0) || (cur_segment < 0) || (cur_segment >= TOT_SEGMENTS) \
                               || (cur_block < 0) || (cur_block >= DATA_BLOCKS_IN_SEGMENT)) {
            logger(ERROR, "[FATAL ERROR] Corrupt file system on disk: invalid checkpoint entry.\n");
            exit(-1);
        }
        
        // Initialize inode table in memory (by simulation).
        memset(inode_table, -1, sizeof(inode_table));
        inode_map imap;
        imap_entry im_entry;
        
        // Traverse all segments.
        int prev_seg_time = 0, prev_seg_block = 0, prev_seg_imap_idx = 0;
        int seg = head_segment;
        bool is_checkpointed = true;
        bool is_break_end = false;
        do {
            struct segment_metadata seg_metadata;
            read_segment_metadata(&seg_metadata, seg);
            int cur_seg_time = seg_metadata.update_time;
            int cur_seg_block = seg_metadata.cur_block;

            if (seg == cur_segment)
                is_checkpointed = false;  // Start to recover un-checkpointed segments.
            if ((is_checkpointed) && (segment_bitmap[seg] == 0)) {
                logger(ERROR, "[FATAL ERROR] Corrupt file system: inconsecutive occupied segments.\n");
                exit(-1);
            }

            if (cur_seg_time < prev_seg_time) {
                if (prev_seg_block == DATA_BLOCKS_IN_SEGMENT-1) {  // Previous segment is full.
                    cur_segment = seg;
                    cur_block = 0;
                    next_imap_index = 0;
                } else {  // Previous segment is still appendable.
                    cur_segment = (seg-1+TOT_SEGMENTS) % TOT_SEGMENTS;
                    cur_block = prev_seg_block;
                    next_imap_index = prev_seg_imap_idx;
                }

                is_break_end = true;
                break;
            } else {
                prev_seg_time = cur_seg_time;
                prev_seg_block = cur_seg_block;

                read_segment_imap(imap, seg);
                for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++) {
                    im_entry = imap[i];
                    if (im_entry.i_number > 0) {
                        // Entries on the "left" are always earlier than those on the "right" (in circular sense).
                        inode_table[im_entry.i_number] = im_entry.inode_block;
                        if (im_entry.i_number > count_inode)
                            count_inode = im_entry.i_number;
                    } else {
                        prev_seg_imap_idx = i;
                        break;
                    }
                }

                if (!is_checkpointed)  // Mark the segment to be occupied.
                    segment_bitmap[seg] = 1;
            } 
            seg = (seg+1) % TOT_SEGMENTS;
        } while (seg != head_segment);

        // In case the disk is full.
        if (!is_break_end)
            if (prev_seg_block == DATA_BLOCKS_IN_SEGMENT-1) {
                    cur_segment = seg;
                    cur_block = 0;
                    next_imap_index = 0;
                } else {
                    cur_segment = (seg-1+TOT_SEGMENTS) % TOT_SEGMENTS;
                    cur_block = prev_seg_block;
                    next_imap_index = prev_seg_imap_idx;
                }

        // Determine whether the file system is already full after recovery.
        if ( ((cur_segment+1) % TOT_SEGMENTS == head_segment)
             && ((cur_block == DATA_BLOCKS_IN_SEGMENT) || (next_imap_index == DATA_BLOCKS_IN_SEGMENT)))
            is_full = true;
        if (is_full) {
            logger(WARN, "[WARNING] The file system is already full. Please run garbage collection to release space.\n");
            // TBD: garbage collection.
        }
        // print_inode_table();
        generate_checkpoint();
        
        // Initialize segment buffer in memory.
        read_segment(segment_buffer, cur_segment);
        memset(segment_buffer + cur_block*BLOCK_SIZE, 0, (DATA_BLOCKS_IN_SEGMENT-cur_block)*BLOCK_SIZE);
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
    // generate_checkpoint(); 
    
    // print_inode_table();
}