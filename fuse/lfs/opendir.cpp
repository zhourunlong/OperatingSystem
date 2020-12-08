#include "opendir.h"
#include "prefix.h"

int o_opendir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "OPENDIR, %s, %p\n", resolve_prefix(path), fi);

    int locate_err = locate(path, fi->fh);
    if (locate_err != 0)
        return locate_err;

    return 0;
}
