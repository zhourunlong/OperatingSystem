#ifndef utimens_h
#define utimens_h

#include <fuse.h>  /* fuse_file_info */

int o_utimens(const char*, const struct timespec[2], struct fuse_file_info*);

#endif
