#include "readdir.h"
#include "prefix.h"
#include "index.h"

int o_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    logger(DEBUG, "READDIR, %s, %p, %p, %d, %p, %d\n",
        resolve_prefix(path), buf, &filler, offset, fi, flags);
    
    int opendir_err = o_opendir(path, fi);
    if (opendir_err != 0)
        return opendir_err;

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);
    
    struct block = read_from_xxx(fi->fh);
    while (block) {
        filler(buf, options.filename, NULL, 0, (fuse_fill_dir_flags)0);
        block = read_
    }
    
    return 0;
}
