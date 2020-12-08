#include "readdir.h"
#include "prefix.h"
#include "index.h"

int o_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    logger(DEBUG, "READDIR, %s, %p, %p, %d, %p, %d\n",
        resolve_prefix(path), buf, &filler, offset, fi, flags);

    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, options.filename, NULL, 0, (fuse_fill_dir_flags)0);
    
    return 0;
}
