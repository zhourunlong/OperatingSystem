#ifndef blockio_h
#define blockio_h

int get_block(void* data, int block_addr);
int new_block(void* data);
void generate_checkpoint(void);

#endif