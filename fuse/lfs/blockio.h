#ifndef blockio_h
#define blockio_h

int locate(char* path, int &i_number);
int get_block(void* data, int block_addr);
int new_block(void* data);

#endif