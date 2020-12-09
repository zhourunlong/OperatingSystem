#ifndef blockio_h
#define blockio_h

int get_block(void* data, int block_addr);
int new_data_block(void* data, int i_number, int direct_index);
int new_inode_block(void* data, int i_number);
int add_segbuf_summary(int cur_block, int i_number, int direct_index);
void generate_checkpoint(void);

#endif