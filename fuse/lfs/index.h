#ifndef index_h
#define index_h

#include <fuse.h>  /* fuse_main */
#include <cassert>
#include <cstddef>

extern struct fuse_operations ops;

void show_help(const char *);

#endif
