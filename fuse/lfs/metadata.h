#ifndef getattr_h
#define getattr_h

#include <sys/stat.h> /* stat */

int o_getattr(const char*, struct stat*, struct fuse_file_info*);
int o_access(const char*, int);

#endif
