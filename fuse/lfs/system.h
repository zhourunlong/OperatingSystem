#ifndef system_h
#define system_h

#include <fuse.h> /* fuse_conn_info */

void* o_init(struct fuse_conn_info*, struct fuse_config*);
void o_destroy(void*);

void initialize_system();

#endif