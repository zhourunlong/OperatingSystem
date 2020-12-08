#include "open.h"
#include "prefix.h"
#include "index.h"

int o_open(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);
    
    if (strcmp(path+1, options.filename) != 0)
		    return -ENOENT;

	  if ((fi->flags & O_ACCMODE) != O_RDONLY)
		    return -EACCES;

    return 0;
}
