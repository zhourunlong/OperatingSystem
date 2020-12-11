#ifndef blockio_h
#define blockio_h

const long USER_DEVICE = 0;

/* High-level functions should ONLY call these interfaces for data transfer. */
void get_block(void* data, int block_addr);
int get_inode_from_inum(void* data, int i_number);

void file_initialize(struct inode* cur_inode, int _mode, int _permission);
void file_add_data(struct inode* cur_inode, void* data);
void file_modify(struct inode* cur_inode, int direct_index, void* data);

/* A typical procedure to add new files:
 * struct inode* cur_inode;
 * file_initialize(cur_inode, _mode, _perm);
 * file_add_data(cur_inode, data);
 * ... (add more data to the file);
 * new_inode_block(cur_inode); */

/* A typical procedure to modify existing files:
 * get_inode_from_inum(cur_inode, i_number);
 * file_modify(cur_inode, direct_index, data);
 * ... (modify more blocks);
 * new_inode_block(cur_inode); */


/* Some lower-level functions that are NOT recommended to be called by users. */
int new_data_block(void* data, int i_number, int direct_index);
int new_inode_block(struct inode* data);
void add_segbuf_summary(int cur_block, int _i_number, int _direct_index);
void add_segbuf_imap(int _i_number, int _block_addr);

/* Periodical checkpoint generator. */
void generate_checkpoint();

#endif