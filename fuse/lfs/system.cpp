#include "system.h"

#include "logger.h"
#include "utility.h"
#include "blockio.h"
#include "path.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>

extern char* current_working_dir;

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    if (DEBUG_PRINT_COMMAND)
        logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;


    /* ****************************************
     * Create / open LFS from disk.
     * ****************************************/
    std::string lfs_path = current_working_dir;
    lfs_path += "lfs.data";

    if (access(lfs_path.c_str(), R_OK) != 0) {    // Disk file does not exist.
        logger(DEBUG, "[INFO] Disk file (lfs.data) does not exist. Try to create and initialize to 0.\n");
        
        // Create a file.
        file_handle = open(lfs_path.c_str(), O_RDWR | O_CREAT, 0777);
        if (file_handle <= 0) {
            logger(ERROR, "[FATAL ERROR] Fail to create a new disk file (lfs.data).\n");
            exit(-1);
        }
        logger(DEBUG, "[INFO] Successfully created a new disk file (lfs.data).\n");

        // Fill 0 into the file.
        char* buf = (char*) malloc(FILE_SIZE);
        memset(buf, 0, FILE_SIZE);
        pwrite(file_handle, buf, FILE_SIZE, 0);
        free(buf);

        // Initialize global state variables.
        memset(segment_bitmap, 0, sizeof(segment_bitmap));
        count_inode = 0; 
        cur_segment = 0;
        cur_block = 0;
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

        // Generate first checkpoint.
        generate_checkpoint();
        checkpoints ckpt;
        read_checkpoints(&ckpt);
        print(ckpt);
        

        logger(DEBUG, "[INFO] Successfully initialized the file system.\n");
    } else {
        // Open an existing file.
        file_handle = open(lfs_path.c_str(), O_RDWR);
        if (file_handle <= 0) {
            logger(ERROR, "[FATAL ERROR] Fail to open existing disk file (lfs.data). Please contact administrator.\n");
            exit(-1);
        }
        logger(DEBUG, "[INFO] Successfully opened an existing file system.\n");


        // Read the (newer) checkpoint.
        checkpoints ckpt;
        read_checkpoints(&ckpt);
        print(ckpt);

        int latest_index = 0;
        if (ckpt[0].timestamp < ckpt[1].timestamp)
            latest_index = 1;
        next_checkpoint = 1 - latest_index;

        memcpy(segment_bitmap, ckpt[latest_index].segment_bitmap, sizeof(segment_bitmap));
        count_inode = ckpt[latest_index].count_inode;
        cur_segment = ckpt[latest_index].cur_segment;
        cur_block = ckpt[latest_index].cur_block;
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
        for (int seg=0; seg<TOT_SEGMENTS; seg++) {
            if (segment_bitmap[seg] == 1) {
                read_segment_imap(imap, seg);
                print(imap);
                for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++) {
                    im_entry = imap[i];
                    if ((im_entry.i_number > 0) && (im_entry.inode_block >= 0)) {
                        // Here we simply regard that segments on the right are newer than those on the left.
                        // However, this is not necessarily true when we perform garbage collection.
                        inode_table[im_entry.i_number] = im_entry.inode_block;
                    }
                }
            }
        }
        print_inode_table();

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
    write_segment(segment_buffer, cur_segment);
    segment_bitmap[cur_segment] = 1;
    generate_checkpoint();
}