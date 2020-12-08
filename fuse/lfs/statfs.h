#ifndef statfs_h
#define statfs_h

#include <sys/statvfs.h>  /* statvfs */

int o_statfs(const char*, struct statvfs*);

#endif
