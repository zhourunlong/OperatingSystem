#include "print.h"

#include "blockio.h"
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

void print(segment_summary seg_sum) {
    logger(DEBUG, "\n[DEBUG] ******************** PRINT SEGMENT SUMMARY ********************\n");
    
    for (int i=0; i<10; i++)
        logger(DEBUG, "       %2d         ", i+1);
    logger(DEBUG, "\n");

    for (int i=0; i<10; i++)
        logger(DEBUG, " IDX  INUM   BLK  ");
    logger(DEBUG, "\n");

    for (int i=0; i<10; i++)
        logger(DEBUG, "==== ====== ====  ");
    logger(DEBUG, "\n");

    int b = 0, count = 0;
    while (b < DATA_BLOCKS_IN_SEGMENT) {
        for (int i=0; i<10; i++) {
            if (true || seg_sum[b].i_number > 0) {
                logger(DEBUG, "%4d %6d %4d  ", b, seg_sum[b].i_number, seg_sum[b].direct_index);
                count++;
            }

            b++;
            if (b == DATA_BLOCKS_IN_SEGMENT)
                break;
        }
        logger(DEBUG, "\n");
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
        for (int j=0; j<10; j++) {
            logger(DEBUG, "%d", ckpt[0].segment_bitmap[i+j]);
            if (i+j+1 == TOT_SEGMENTS) break;
        }
        logger(DEBUG, "  \t");
        for (int j=0; j<10; j++) {
            logger(DEBUG, "%d", ckpt[1].segment_bitmap[i+j]);
            if (i+j+1 == TOT_SEGMENTS) break;
        }
        logger(DEBUG, "\n");
        i += 10;
    }
    
    logger(DEBUG, "COUNT_INODE\t%d   \t\t%d\n", ckpt[0].count_inode, ckpt[1].count_inode);
    logger(DEBUG, "IS_FULL    \t%d   \t\t%d\n", ckpt[0].is_full, ckpt[1].is_full);
    logger(DEBUG, "CUR_SEGMENT\t%d   \t\t%d\n", ckpt[0].cur_segment, ckpt[1].cur_segment);
    logger(DEBUG, "CUR_BLOCK  \t%d   \t\t%d\n", ckpt[0].cur_block, ckpt[1].cur_block);
    logger(DEBUG, "NXT_IMAP_ID\t%d   \t\t%d\n", ckpt[0].next_imap_index, ckpt[1].next_imap_index);
    logger(DEBUG, "TMSTMP_SEC \t%d\t%d\n", ckpt[0].timestamp_sec, ckpt[1].timestamp_sec);
    logger(DEBUG, "TMSTMP_NSEC\t%d\t%d\n", ckpt[0].timestamp_nsec, ckpt[1].timestamp_nsec);
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
    } else if (disp == DISP_BYTE_CHAR) {    // Display byte-by-byte in characters.
        for (int b=0; b<BLOCK_SIZE; b++)
            logger(DEBUG, "%c", (unsigned char)blk[b]);
        logger(DEBUG, "\n");
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
    
    logger(DEBUG, " I_NUM  BLOCK   I_NUM  BLOCK   I_NUM  BLOCK   I_NUM  BLOCK   I_NUM  BLOCK\n");
    logger(DEBUG, "====== ======  ====== ======  ====== ======  ====== ======  ====== ======\n");
    
    int count = 0, col = 0;
    for (int i=0; i<MAX_NUM_INODE; i++) {
        if (inode_table[i] >= 0) {
            logger(DEBUG, "%6d %6d  ", i, inode_table[i]);
            count++;
            col++;
            if (col == 5) {
                col = 0;
                logger(DEBUG, "\n");
            }
        }
    }

    logger(DEBUG, "\nThere are %d active inodes in LFS now.\n", count);
    logger(DEBUG, "============================ PRINT INODE TABLE ====================\n\n");
}

void print_util_stat(struct util_entry* util) {
    logger(DEBUG, "\n[DEBUG] ******************** UTILIZATION STAT ********************\n");

    logger(DEBUG, "    ");
    for (int j=0; j<10; j++)
        logger(DEBUG, "        %2d  ", j);
    logger(DEBUG, "\n");
    logger(DEBUG, "    ");
    for (int j=0; j<10; j++)
        logger(DEBUG, "==========  ");
    logger(DEBUG, "\n");

    int i = 0, row = 0;
    while (i < TOT_SEGMENTS) {
        logger(DEBUG, "%2d  ", row);
        row++;

        for (int j=0; j<10; j++) {
            logger(DEBUG, "%4d(%4d)  ", util[i].segment_number, util[i].count);
            i++;
            if (i == TOT_SEGMENTS) break;
        }
        logger(DEBUG, "\n");
    }
    logger(DEBUG, "============================ UTILIZATION STAT ====================\n\n");
}

void print_time_stat(struct time_entry* ts) {
    logger(DEBUG, "\n[DEBUG] ******************** TIMESTAMP STAT ********************\n");

    logger(DEBUG, "    ");
    for (int j=0; j<10; j++)
        logger(DEBUG, "  %2d  ", j);
    logger(DEBUG, "\n");
    logger(DEBUG, "    ");
    for (int j=0; j<10; j++)
        logger(DEBUG, "====  ");
    logger(DEBUG, "\n");

    int i = 0, row = 0;
    while (i < TOT_SEGMENTS) {
        logger(DEBUG, "%2d  ", row);
        row++;

        for (int j=0; j<10; j++) {
            logger(DEBUG, "%4d  ", ts[i].segment_number);
            i++;
            if (i == TOT_SEGMENTS) break;
        }
        logger(DEBUG, "\n");
    }
    logger(DEBUG, "============================ TIMESTAMP STAT ====================\n\n");
}


void interactive_debugger() {
    logger(DEBUG, "\n********************************************************************************\n");
    logger(DEBUG, "***************************** INTERACTIVE DEBUGGER *****************************\n");
    logger(DEBUG, "********************************************************************************\n\n");
    char token;
    int op_num;

    printf("[USAGE: q = quit, i = inode, f = file data, d = directory, t = inode table]\n");
    printf("(debugger) >>> ");
    scanf("\n%c", &token);
    while (token != 'q') {
        if (token == 'i') {
            scanf("%d", &op_num);
            struct inode* _inode;
            get_inode_from_inum(_inode, op_num);
            print(_inode);
        } else if (token == 'f') {
            scanf("%d", &op_num);
            block _block;
            get_block(_block, op_num);
            print(_block, DISP_BYTE_CHAR);
        } else if (token == 'd') {
            scanf("%d", &op_num);
            directory _block;
            get_block(&_block, op_num);
            print(_block);
        } else if (token == 't') {
            print_inode_table();
        } else if (token == 'q') {
            break;
        } else {
            printf("Invalid token %c.\n", token);
        }

        printf("[USAGE: q = quit, i = inode, f = file data, d = directory, t = inode table]\n");
        printf("(debugger) >>> ");
        scanf("\n%c", &token);
    }

    logger(DEBUG, "\n================================================================================\n");
    logger(DEBUG, "============================= INTERACTIVE DEBUGGER =============================\n");
    logger(DEBUG, "================================================================================\n\n");
}