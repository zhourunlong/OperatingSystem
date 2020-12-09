#ifndef blockio_h
#define blockio_h

const int USER_UID = 0;
const int USER_GID = 0;
const int USER_DEVICE = 0;

/* High-level functions should ONLY call these interfaces for data transfer. */
void get_block(void* data, int block_addr);
void file_initialize(struct inode* cur_inode, int mode, int permission);
void file_add_data(struct inode* cur_inode, void* data);
void file_commit(struct inode* cur_inode);
/* High-level functions should ONLY call these interfaces for data transfer. */

int new_data_block(void* data, int i_number, int direct_index);
int new_inode_block(void* data, int i_number);
void add_segbuf_summary(int cur_block, int i_number, int direct_index);
void add_segbuf_imap(int i_number, int block_addr);

void generate_checkpoint(void);

#endif