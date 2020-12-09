#include "metadata.h"

#include "logger.h"
#include "path.h"
#include "index.h"

#include <cstring>

extern struct options options;

int o_getattr(const char* path, struct stat* sbuf, struct fuse_file_info* fi) {
    logger(DEBUG, "GETATTR, %s, %p, %p\n", resolve_prefix(path), sbuf, fi);
    

	int flag = 0;

	memset(sbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		sbuf->st_mode = S_IFDIR | 0755;
		sbuf->st_nlink = 2;
	} else if (strcmp(path+1, options.filename) == 0) {
		sbuf->st_mode = S_IFREG | 0444;
		sbuf->st_nlink = 1;
		sbuf->st_size = strlen(options.contents);
	} else
		flag = -ENOENT;

	return flag;
}


int o_access(const char* path, int mode) {
    logger(DEBUG, "ACCESS, %s, %d\n", resolve_prefix(path), mode);
    return 0;
}