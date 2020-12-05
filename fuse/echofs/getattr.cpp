#include "getattr.h"

int o_getattr(const char* path, struct stat* sbuf, struct fuse_file_info* fi) {
  logger(ERROR, "UNIMPLEMENTED: getattr, path: %s\n", path);
  return -1;
}

