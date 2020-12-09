#ifndef blockio_h
#define blockio_h

const int USER_UID = 0;
const int USER_GID = 0;
const long USER_DEVICE = 0;

/* High-level functions should ONLY call these interfaces for data transfer. */
void get_block(void* data, int block_addr);
void get_inode_from_inum(void* data, int i_number);

// A typical procedure to add new files:
// + struct inode* cur_inode = (struct inode*) malloc(sizeof(struct inode));
// + file_initialize(cur_inode, _mode, _perm);
// + file_add_data(cur_inode, data);
// - (add more data to the file);
// - (for a modified file, "next_indirect" of the inode should be manually copied before commitment)
// + file_commit(cur_inode);
void file_initialize(struct inode* cur_inode, int _mode, int _permission);
void file_add_data(struct inode* cur_inode, void* data);
void file_commit(struct inode* cur_inode);
/* High-level functions should ONLY call these interfaces for data transfer. */

int new_data_block(void* data, int i_number, int direct_index);
int new_inode_block(void* data, int i_number);
void add_segbuf_summary(int cur_block, int _i_number, int _direct_index);
void add_segbuf_imap(int _i_number, int _block_addr);

void generate_checkpoint(void);

#endif