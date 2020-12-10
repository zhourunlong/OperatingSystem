#include "extra.h"

#include "logger.h"

int o_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi,
            unsigned int flags, void* data){
    logger(DEBUG, "IOCTL\n");
    return 0;
}