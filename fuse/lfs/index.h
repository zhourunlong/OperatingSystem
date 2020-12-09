#ifndef index_h
#define index_h

#include <fuse.h>  /* fuse_main */
#include <cassert>
#include <cstddef>

extern struct fuse_operations ops;

struct options {
	const char *filename;
	const char *contents;
	int show_help;
};

extern struct options options;

extern const struct fuse_opt option_spec[];

void show_help(const char *);

#endif
