#ifndef perm
#define perm

#include <sys/stat.h>
#include <unistd.h>
int o_chmod(const char*, mode_t, struct fuse_file_info*);
int o_chown(const char*, uid_t, gid_t, struct fuse_file_info*);

#endif