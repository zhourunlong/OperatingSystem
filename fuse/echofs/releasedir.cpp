#include "releasedir.h"
#include "prefix.h"

int o_releasedir(const char* path, struct fuse_file_info* fi) {
    logger(DEBUG, "RELEASEDIR, %s, %p\n", resolve_prefix(path), fi);

    return 0;
}
