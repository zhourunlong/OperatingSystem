#include "logger.h" /* logger */
#include "utility.h"
#include "prefix.h"
#include <fuse.h> /* fuse_conn_info */
#include "init.h"
#include "unistd.h"
#include "string.h"
#include <string>

extern char* current_working_dir;

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;

    /* ******************************
     * Create / open LFS from disk.
     * ******************************/
    std::string lfs_path = current_working_dir;
    lfs_path += "lfs.data";
    if (access(lfs_path.c_str(), R_OK) != 0) {
        logger(WARN, "Disk file does not exist. Try to create and initialize to 0.\n");
        
        // Create a file.
        file_handle = open(lfs_path.c_str(), O_RDWR | O_CREAT, 0777);

        // Fill 0 into the file.
        char* buf = (char*) malloc(FILE_SIZE);
        memset(buf, 0, FILE_SIZE);
        pwrite(file_handle, buf, FILE_SIZE, 0);
        free(buf);
        logger(DEBUG, "Successfully initialized.\n");

        // Initialize superblock.
        struct superblock init_sblock = {
            tot_inodes   : 100000,
            tot_blocks   : 102400,
            tot_segments : 100,
            block_size   : 1024,
            segment_size : 1048576
        };
        write_superblock(&init_sblock);
    } else {
        // Open an existing file.
        file_handle = open(lfs_path.c_str(), O_RDWR);
        logger(DEBUG, "Successfully opened an existing file.\n");
    }

    /* ******************************
     * Initialize (simulated) inode table in memory.
     * ******************************/
    checkpoints ckpt;
    read_checkpoints(&ckpt);

    int latest_index = 0;
    if (ckpt[0].timestamp < ckpt[1].timestamp)
        latest_index = 1;

    char* seg_bitmap = ckpt[latest_index].segment_bitmap;
    count_inode = ckpt[latest_index].count_inode;
    cur_segment = ckpt[latest_index].cur_segment;
    cur_block = ckpt[latest_index].cur_block;

    memset(inode_table, 0, sizeof(inode_table));
    for (int seg=0; seg<TOT_SEGMENTS; seg++) {
        if (seg_bitmap[seg] == 1) {
            inode_map imap;
            imap_entry im_entry;

            read_segment_imap(imap, seg);
            for (int i=0; i<MAX_IMAP_ENTRIES; i++) {
                im_entry = imap[i];
                if ((im_entry.i_number > 0) && (im_entry.inode_block > 0)) {
                    // Here we simply regard that segments on the right are newer than those on the left.
                    // However, this is not necessarily true when we perform garbage collection.
                    inode_table[im_entry.i_number] = im_entry.inode_block;
                }
            }
        }
    }

    /* ******************************
     * Initialize segment buffer in memory.
     * ******************************/
    memset(segment_buffer, 0, sizeof(segment_buffer));
    
    
	return NULL;
}
