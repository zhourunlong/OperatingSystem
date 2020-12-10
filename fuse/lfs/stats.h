#ifndef stats_h
#define stats_h

#include <sys/statvfs.h> 
#include <fuse.h>

int o_statfs(const char*, struct statvfs*);
int o_utimens(const char*, const struct timespec[2], struct fuse_file_info*);

#endif
