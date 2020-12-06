#include "init.h"

void* o_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    logger(DEBUG, "INIT, %p, %p\n", conn, cfg);

    (void) conn;
	cfg->kernel_cache = 1;
    
	return NULL;
}
