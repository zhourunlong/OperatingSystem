#include "file.h"

#include "logger.h"
#include "path.h"
#include "index.h"
#include "errno.h"

#include <cstring>

extern struct options options;


int o_open(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPEN, %s, %p\n", resolve_prefix(path), fi);
    
    if (strcmp(path+1, options.filename) != 0)
		    return -ENOENT;

	  if ((fi->flags & O_ACCMODE) != O_RDONLY)
		    return -EACCES;

    return 0;
}

int o_release(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "RELEASE, %s, %p\n", resolve_prefix(path), fi);
    return 0;
}

int o_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
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

int o_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    logger(DEBUG, "WRITE, %s, %p, %d, %d, %p\n",
        resolve_prefix(path), buf, size, offset, fi);
    return 0;
}

int o_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    logger(DEBUG, "CREATE, %s, %d, %p\n",
        resolve_prefix(path), mode, fi);
    return 0;
}

int o_rename(const char* from, const char* to, unsigned int flags) {
    logger(DEBUG, "RENAME, %s, %s, %d\n",
        resolve_prefix(from), resolve_prefix(to), flags);
    return 0;
}

int o_unlink(const char* path) {
    logger(DEBUG, "UNLINK, %s\n", resolve_prefix(path));
    return 0;
}

int o_link(const char* src, const char* dest) {
    logger(DEBUG, "LINK, %s, %s\n", resolve_prefix(src), resolve_prefix(dest));
    return 0;
}

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
    logger(DEBUG, "TRUNCATE, %s, %d, %p\n",
        resolve_prefix(path), size, fi);
    return 0;
}