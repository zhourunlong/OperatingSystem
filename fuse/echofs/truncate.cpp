#include "truncate.h"

int o_truncate(const char* path, off_t size, struct fuse_file_info *fi) {
  logger(ERROR, "UNIMPLEMENTED: truncate, path: %s\n", path);
  return -1;
}
