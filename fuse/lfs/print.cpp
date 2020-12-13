#include "print.h"

#include "logger.h"

/** **************************************
 * Pretty-print functions.
 * ***************************************/
void print(struct inode* node){
    logger(DEBUG, "\n[DEBUG] ******************** PRINT INODE ********************\n");
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
    logger(DEBUG, "SIZE\t%d B, %d blocks (IO = %d blocks)\n", node->fsize_byte, node->fsize_block, node->io_block);
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
    logger(DEBUG, "============================ PRINT INODE ====================\n\n");
}

void print(directory dir) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT DIRECTORY ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tNAME        \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<MAX_DIR_ENTRIES; i++)
        if (dir[i].i_number > 0) {
            logger(DEBUG, "%d\t%d\t%s\n", i, dir[i].i_number, dir[i].filename);
            count++;
        }
    logger(DEBUG, "\nThere are %d files / sub-directories in this directory.\n", count);
    logger(DEBUG, "============================ PRINT DIRECTORY ====================\n\n");
}

void print(inode_map imap) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT INODE MAP ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tBLOCK_ADDR  \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++)
        if ((imap[i].i_number > 0) && (imap[i].inode_block >= 0)) {
            logger(DEBUG, "%d\t%d\t%d\n", i, imap[i].i_number, imap[i].inode_block);
            count++;
        }
    logger(DEBUG, "\nThere are %d inodes in this segment.\n", count);
    logger(DEBUG, "============================ PRINT INODE MAP ====================\n\n");
}

void print(segment_summary segsum) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT SEGMENT SUMMARY ********************\n");
    logger(DEBUG, "IDX\tI_NUM \tDIRECT[?]   \n");
    logger(DEBUG, "===\t======\t============\n");
    int count = 0;
    for (int i=0; i<DATA_BLOCKS_IN_SEGMENT; i++)
        if (segsum[i].i_number > 0) {
            logger(DEBUG, "%d\t%d\t%s\n", i, segsum[i].i_number, segsum[i].direct_index);
            count++;
        }
    logger(DEBUG, "\nThere are %d non-empty blocks in this segment.\n", count);
    logger(DEBUG, "============================ PRINT SEGMENT SUMMARY ====================\n\n");
}

void print(struct superblock* sblk) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT SUPERBLOCK ********************\n");
    logger(DEBUG, "ITEM      \tCONTENT     \n");
    logger(DEBUG, "==========\t============\n");
    logger(DEBUG, "TOT_INODES  \t%d\n", sblk->tot_inodes);
    logger(DEBUG, "TOT_BLOCKS  \t%d\n", sblk->tot_blocks);
    logger(DEBUG, "TOT_SEGMENTS\t%d\n", sblk->tot_segments);
    logger(DEBUG, "BLOCK_SIZE  \t%d\n", sblk->block_size);
    logger(DEBUG, "SEGMENT_SIZE\t%d\n", sblk->segment_size);
    logger(DEBUG, "============================ PRINT SUPERBLOCK ====================\n\n");
}

void print(checkpoints ckpt){
    logger(DEBUG, "\n[DEBUG] ******************** PRINT CHECKPOINTS ********************\n");
    logger(DEBUG, "ITEM       \tCKPT[0]     \tCKPT[1]     \n");
    logger(DEBUG, "========== \t============\t============\n");

    logger(DEBUG, "SEG_BITMAP \t");
    int i = 0;
    while (i < TOT_SEGMENTS) {
        if (i>0) logger(DEBUG, "           \t");
        for (int j=0; j<10; j++)
            logger(DEBUG, "%d", ckpt[0].segment_bitmap[i+j]);
        logger(DEBUG, "  \t");
        for (int j=0; j<10; j++)
            logger(DEBUG, "%d", ckpt[1].segment_bitmap[i+j]);
        logger(DEBUG, "\n");
        i += 10;
    }
    
    logger(DEBUG, "COUNT_INODE\t%d   \t\t%d\n", ckpt[0].count_inode, ckpt[1].count_inode);
    logger(DEBUG, "1ST_SEGMENT\t%d   \t\t%d\n", ckpt[0].head_segment, ckpt[1].head_segment);
    logger(DEBUG, "CUR_SEGMENT\t%d   \t\t%d\n", ckpt[0].cur_segment, ckpt[1].cur_segment);
    logger(DEBUG, "CUR_BLOCK  \t%d   \t\t%d\n", ckpt[0].cur_block, ckpt[1].cur_block);
    logger(DEBUG, "NXT_IMAP_ID\t%d   \t\t%d\n", ckpt[0].next_imap_index, ckpt[1].next_imap_index);
    logger(DEBUG, "TIMESTAMP  \t%d\t%d\n", ckpt[0].timestamp, ckpt[1].timestamp);
    logger(DEBUG, "============================ PRINT CHECKPOINTS ====================\n\n");
}

void print(block blk, int disp) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT DATA BLOCK ********************\n");

    if (disp == DISP_BIT_BIN) {    // Display bit-by-bit, grouped in 8 bits, 16 bytes in a row.
        logger(DEBUG, "\t");
        for (int i=0; i<16; i++)
            logger(DEBUG, "%d\t ", 8*i);
        logger(DEBUG, "\n");

        logger(DEBUG, "\t");
        for (int i=0; i<16; i++)
            logger(DEBUG, "======== ");
        logger(DEBUG, "\n");

        int b = 0;
        int count = 0;
        while (b < BLOCK_SIZE) {
            logger(DEBUG, "%6d\t", count);
            for (int i=0; i<16; i++) {
                int dec = blk[b];
                int bin = 0;
                int pow = 1;
                for (int j=0; j<8; j++) {
                    bin = bin + (dec%2) * pow;
                    pow *= 10;
                    dec /= (int)2;
                }
                logger(DEBUG, "%d ", bin);

                b++;
                if (b >= BLOCK_SIZE) break;
            }
            logger(DEBUG, "\n");

            count++;
        }
    } else if (disp == DISP_BYTE_DEC) {    // Display byte-by-byte in base-10, 32 bytes in a row.
        logger(DEBUG, "    ");
        for (int i=0; i<32; i++)
            logger(DEBUG, "%4d", i);
        logger(DEBUG, "\n");

        logger(DEBUG, "    ");
        for (int i=0; i<32; i++)
            logger(DEBUG, " ===");
        logger(DEBUG, "\n");

        int b = 0;
        int count = 0;
        while (b < BLOCK_SIZE) {
            logger(DEBUG, "%4d", count);
            for (int i=0; i<32; i++) {
                logger(DEBUG, "%4d", blk[b]);
                b++;
            }
            logger(DEBUG, "\n");

            count++;
        }
    } else if (disp == DISP_BYTE_HEX) {    // Display byte-by-byte in base-16, 32 bytes in a row.
        logger(DEBUG, "    ");
        for (int i=0; i<32; i++)
            logger(DEBUG, "%4d", i);
        logger(DEBUG, "\n");

        logger(DEBUG, "    ");
        for (int i=0; i<32; i++)
            logger(DEBUG, " ===");
        logger(DEBUG, "\n");

        int b = 0;
        int count = 0;
        while (b < BLOCK_SIZE) {
            logger(DEBUG, "%4d", count);
            for (int i=0; i<32; i++) {
                logger(DEBUG, "%4x", (unsigned char)blk[b]);
                b++;
            }
            logger(DEBUG, "\n");

            count++;
        }
    } else if (disp == DISP_WORD_DEC) {    // Display word-by-word in base-10, 16 words in a row.
        logger(DEBUG, "    ");
        for (int i=0; i<16; i++)
            logger(DEBUG, "%12d", i);
        logger(DEBUG, "\n");

        logger(DEBUG, "    ");
        for (int i=0; i<16; i++)
            logger(DEBUG, "  ==========");
        logger(DEBUG, "\n");

        int* _blk = (int*) blk;
        int b = 0;
        int count = 0;
        while (b < BLOCK_SIZE / (int)4) {
            logger(DEBUG, "%4d", count);
            for (int i=0; i<16; i++) {
                logger(DEBUG, "%12d", _blk[b]);
                b++;
            }
            logger(DEBUG, "\n");

            count++;
        }
    } else if (disp == DISP_WORD_HEX) {    // Display word-by-word in base-16, 16 words in a row.
        logger(DEBUG, "    ");
        for (int i=0; i<16; i++)
            logger(DEBUG, "%10d", i);
        logger(DEBUG, "\n");

        logger(DEBUG, "    ");
        for (int i=0; i<16; i++)
            logger(DEBUG, "  ========");
        logger(DEBUG, "\n");

        int* _blk = (int*) blk;
        int b = 0;
        int count = 0;
        while (b < BLOCK_SIZE / (int)4) {
            logger(DEBUG, "%4d", count);
            for (int i=0; i<16; i++) {
                logger(DEBUG, "%10x", (unsigned char)_blk[b]);
                b++;
            }
            logger(DEBUG, "\n");

            count++;
        }
    }
    logger(DEBUG, "============================ PRINT DATA BLOCK ====================\n\n");
}

void print_inode_table() {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT INODE TABLE ********************\n");
    
    logger(DEBUG, "I_NUMBER  \tBLOCK_ADDR  \n");
    logger(DEBUG, "==========\t============\n");
    
    int count = 0;
    for (int i=0; i<MAX_NUM_INODE; i++) {
        if (inode_table[i] >= 0) {
            logger(DEBUG, "%d\t\t%d\n", i, inode_table[i]);
            count++;
        }
    }

    logger(DEBUG, "\nThere are %d active inodes in LFS now.\n", count);
    logger(DEBUG, "============================ PRINT INODE TABLE ====================\n\n");
}