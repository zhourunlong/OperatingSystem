#include "rename.h"
#include "prefix.h"

int o_rename(const char* from, const char* to, unsigned int flags) {
    logger(DEBUG, "RENAME, %s, %s, %d\n",
        resolve_prefix(from), resolve_prefix(to), flags);
    return 0;
}
