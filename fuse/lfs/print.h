#ifndef print_h
#define print_h

#include "utility.h"
#include "cleaner.h"

/** **************************************
 * Pretty-print functions.
 * ***************************************/
void print(struct inode* node);
void print(directory dir);
void print(inode_map imap);
void print(segment_summary seg_sum);
void print(struct superblock* sblk);
void print(checkpoints ckpt);

const int DISP_BIT_BIN   = 1;
const int DISP_BYTE_DEC  = 2;
const int DISP_BYTE_HEX  = 3;
const int DISP_BYTE_CHAR = 4;
const int DISP_WORD_DEC  = 5;
const int DISP_WORD_HEX  = 6;
void print(block blk, int disp);

void print_inode_table();
void print_util_stat(struct util_entry* util);
void print_time_stat(struct time_entry* ts);
void interactive_debugger();

#endif