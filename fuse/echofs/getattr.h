#ifndef getattr_h
#define getattr_h
#include "./logger.h" /* logger */
#include <sys/stat.h> /* stat */
extern struct options options;
int o_getattr(const char*, struct stat*, struct fuse_file_info*);
#endif
