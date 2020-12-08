#ifndef blockio_h
#define blockio_h

int locate(char* path, int &i_number);
int get_block(int block_addr, void* data);
int new_block(void* data);

#endif