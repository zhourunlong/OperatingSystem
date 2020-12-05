#include "mkdir.h"
#include "prefix.h"

int o_mkdir(const char* path, mode_t mode) {
    logger(DEBUG, "MKDIR, %s, %d\n", resolve_prefix(path), mode);
    return 0;
}
