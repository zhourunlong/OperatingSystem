#ifndef file_h
#define file_h

#include <fuse.h> /* fuse_file_info */
#include <unistd.h> /* uid_t gid_t */

int o_open(const char*, struct fuse_file_info*);
int o_release(const char*, struct fuse_file_info*);
int o_read(const char*, char*, size_t, off_t, struct fuse_file_info*);
int o_write(const char*, const char*, size_t, off_t, struct fuse_file_info*);
int o_create(const char*, mode_t mode, struct fuse_file_info*);
int o_rename(const char*, const char*, unsigned int);
int o_unlink(const char* path);
int o_link(const char*, const char*);
int o_truncate(const char* path, off_t size, struct fuse_file_info *fi);

#endif
