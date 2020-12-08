#ifndef create_h
#define create_h
#include "./logger.h" /* logger */
#include <unistd.h> /* uid_t gid_t */
int o_create(const char*, mode_t mode, struct fuse_file_info*);
#endif
