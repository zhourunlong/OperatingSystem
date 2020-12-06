#include "read.h"
#include "prefix.h"
#include "index.h"

int o_read(
    const char* path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info* fi
) {
    logger(DEBUG, "READ, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);

    size_t len;
	(void) fi;
	if(strcmp(path+1, options.filename) != 0)
		return -ENOENT;
	len = strlen(options.contents);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, options.contents + offset, size);
	} else
		size = 0;

    return size;
}
