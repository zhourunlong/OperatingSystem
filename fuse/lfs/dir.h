#ifndef dir
#define dir

#include <fuse.h>
#include <sys/stat.h>

int o_opendir(const char*, struct fuse_file_info*);
int o_releasedir(const char*, struct fuse_file_info*);
int o_readdir(
    const char*,
    void*,
    fuse_fill_dir_t,
    off_t,
    struct fuse_file_info*,
    enum fuse_readdir_flags
);
int o_mkdir(const char*, mode_t);
int o_rmdir(const char*);

#endif