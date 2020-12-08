#ifndef index_h
#define index_h

#include "prefix.h" /* resolve_prefix generate_prefix */
#include "init.h" /* o_init */
#include "getattr.h" /* o_getattr */
#include "access.h" /* o_access */
#include "destroy.h" /* o_destroy */
#include "unlink.h" /* o_unlink */
#include "rename.h" /* o_rename */
#include "truncate.h" /* o_truncate */
#include "utimens.h" /* o_utimens */
#include "open.h" /* o_open */
#include "read.h" /* o_read */
#include "write.h" /* o_write */
#include "statfs.h" /* o_statfs */
#include "release.h" /* o_release */
#include "fsync.h" /* o_fsync */
#include "fsyncdir.h" /* o_fsyncdir */
#include "flush.h" /* o_flush */
#include "lock.h" /* o_lock */
#include "create.h" /* o_create */

#include <fuse.h> /* fuse_main */

#include "logger.h" /* set_log_level set_log_output logger */

#include <cassert>
#include <cstddef>

extern struct fuse_operations ops;

extern struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

extern const struct fuse_opt option_spec[];

void show_help(const char *);

#endif
