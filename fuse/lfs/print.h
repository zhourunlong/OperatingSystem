#ifndef print_h
#define print_h

#include "utility.h"

/** **************************************
 * Pretty-print functions.
 * ***************************************/
void print(struct inode* node);
void print(directory dir);
void print(inode_map imap);
void print(segment_summary segsum);
void print(struct superblock* sblk);
void print(checkpoints ckpt);

const int DISP_BIT_BIN = 1;
const int DISP_BYTE_DEC = 2;
const int DISP_BYTE_HEX = 3;
const int DISP_WORD_DEC = 4;
const int DISP_WORD_HEX = 5;
void print(block blk, int disp);

void print_inode_table();

#endif