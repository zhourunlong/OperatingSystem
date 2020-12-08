#include "system.h"

#include "logger.h"
#include "utility.h"
#include "blockio.h"
#include "prefix.h"

#include <unistd.h>
#include <string.h>
#include <string>
#include <time.h>

extern char* current_working_dir;

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;

    /* ******************************
     * Initialize segment buffer in memory.
     * ******************************/
    memset(segment_buffer, 0, sizeof(segment_buffer));

    /* ******************************
     * Create / open LFS from disk.
     * ******************************/
    std::string lfs_path = current_working_dir;
    lfs_path += "lfs.data";
    if (access(lfs_path.c_str(), R_OK) != 0) {
        logger(WARN, "Disk file (lfs.data) does not exist. Try to create and initialize to 0.\n");
        
        // Create a file.
        file_handle = open(lfs_path.c_str(), O_RDWR | O_CREAT, 0777);
        if (file_handle <= 0) {
            logger(ERROR, "Fail to create a new disk file (lfs.data).\n");
            exit(-1);
        }
        logger(DEBUG, "Successfully created a new disk file (lfs.data).\n");

        // Fill 0 into the file.
        char* buf = (char*) malloc(FILE_SIZE);
        memset(buf, 0, FILE_SIZE);
        pwrite(file_handle, buf, FILE_SIZE, 0);
        free(buf);

        // Initialize superblock.
        struct superblock init_sblock = {
            tot_inodes   : TOT_INODES,
            tot_blocks   : BLOCKS_IN_SEGMENT * TOT_SEGMENTS,
            tot_segments : TOT_SEGMENTS,
            block_size   : BLOCK_SIZE,
            segment_size : SEGMENT_SIZE
        };
        write_superblock(&init_sblock);

        // Initialize root directory (i_number = 1).
        char* buf = (char*) malloc(BLOCK_SIZE);
        memset(buf, 0, BLOCK_SIZE);
        new_block(buf);
        free(buf);

        time_t cur_time;
        time(&cur_time);
        inode root_inode = {
            i_number       : 1,
            mode           : 2,
            num_links      : 1,
            fsize_byte     : 0,
            fsize_block    : 1,
            io_block       : 1,
            permission     : 0777,
            perm_uid       : 0,    // ????
            perm_gid       : 0,    // ????
            device         : 0,    // ????
            atime          : 0,    // ????
            mtime          : (int) cur_time,
            ctime          : (int) cur_time
        }
        memset(root_inode.direct, -1, sizeof(root_inode.direct));
        root_inode.direct[0] = 0;
        root_inode.next_indirect = -1;
        new_block(&root_inode);

        count_inode = 1;
        cur_segment = 0;
        cur_block = 2;
        inode_table[1] = 1;
        root_dir_inumber = 1;

        logger(DEBUG, "Successfully initialized the file system.\n");
    } else {
        // Open an existing file.
        file_handle = open(lfs_path.c_str(), O_RDWR);
        if (file_handle <= 0) {
            logger(ERROR, "Fail to open existing disk file (lfs.data).\n");
            exit(-1);
        }
        logger(DEBUG, "Successfully opened an existing file system.\n");


        /* ******************************
        * Read the (newer) checkpoint.
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
        root_dir_inumber = ckpt[latest_index].root_dir_inumber;
        if ((count_inode <= 0) || (cur_segment < 0) || (cur_segment >= TOT_SEGMENTS) \
                               || (cur_block < 0) || (cur_block >= BLOCKS_IN_SEGMENT) \
                               || (root_dir_inumber <= 0) || (root_dir_inumber >= TOT_INODES)) {
            logger(ERROR, "Corrupt file system on disk. Please erase it and re-initiallize.\n");
            exit(-1);
        }


        /* ******************************
        * Initialize (simulated) inode table in memory.
        * ******************************/
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
    }
    
    
	return NULL;
}


void o_destroy(void* private_data) {
    logger(DEBUG, "DESTROY, %p\n", private_data);
}