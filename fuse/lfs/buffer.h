#ifndef buffer_h
#define buffer_h

#include <fuse.h>  /* fuse_file_info */

int o_flush(const char* path, struct fuse_file_info* fi);
int o_fsync(const char*, int, struct fuse_file_info*);
int o_fsyncdir(const char*, int, struct fuse_file_info*);

#endif
