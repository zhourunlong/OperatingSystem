#include "chown.h"

int o_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
  logger(
    ERROR,
    "UNIMPLEMENTED: chown, path: %s, uid: %lu, gid: %lu\n",
    path,
    (unsigned long) uid,
    (unsigned long) gid
  );
  return -1;
}
