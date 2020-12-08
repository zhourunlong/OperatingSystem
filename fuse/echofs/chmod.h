#ifndef chmod_h
#define chmod_h
#include "./logger.h" /* logger */
#include <sys/stat.h> /* mode_t */
int o_chmod(const char*, mode_t, struct fuse_file_info*);
#endif
