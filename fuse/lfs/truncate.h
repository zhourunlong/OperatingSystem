#ifndef truncate_h
#define truncate_h
#include "./logger.h" /* logger */
int o_truncate(const char* path, off_t size, struct fuse_file_info *fi);
#endif
