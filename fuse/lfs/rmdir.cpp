#include "rmdir.h"
#include "prefix.h"

int o_rmdir(const char* path) {
    logger(DEBUG, "RMDIR, %s\n", resolve_prefix(path));
    return 0;
}
