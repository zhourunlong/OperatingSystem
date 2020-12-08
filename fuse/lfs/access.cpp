#include "access.h"
#include "prefix.h"

int o_access(const char* path, int mode) {
    logger(DEBUG, "ACCESS, %s, %d\n", resolve_prefix(path), mode);
    return 0;
}
