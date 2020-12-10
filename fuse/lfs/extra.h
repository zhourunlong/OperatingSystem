#ifndef extra_h
#define extra_h

#include <fuse.h> 
int o_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi,
            unsigned int flags, void* data);

#endif