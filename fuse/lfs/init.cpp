#include "init.h"

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;

    int file_handle = open("/home/user/OS_Lab/lfs.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);
    
	return NULL;
}
