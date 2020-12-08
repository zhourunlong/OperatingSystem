#include "index.h"

#include "system.h"     /* o_init, o_destroy */
#include "metadata.h"   /* o_getattr, o_access */
#include "file.h"       /* o_open, o_release, o_read, o_write, o_create, o_rename, o_unlink, o_link, o_truncate */
#include "dir.h"        /* o_opendir, o_releasedir, o_readdir, o_mkdir, o_rmdir */
#include "perm.h"       /* o_chmod, o_chown */
#include "buffer.h"     /* o_flush, o_fsync, o_fsyncdir */
#include "lock.h"       /* o_lock */

#include "utimens.h"    /* o_utimens */
#include "statfs.h"     /* o_statfs */

#include "prefix.h"     /* resolve_prefix generate_prefix */
#include "logger.h"     /* set_log_level set_log_output logger */

struct fuse_operations ops = {
    .getattr    = o_getattr,
    .mkdir      = o_mkdir,
    .unlink     = o_unlink,
    .rmdir      = o_rmdir,
    .rename     = o_rename,
    .link       = o_link
    .chmod      = o_chmod,
    .chown      = o_chown,
    .truncate   = o_truncate,
    .open       = o_open,
    .read       = o_read,
    .write      = o_write,
    .statfs     = o_statfs,
    .flush      = o_flush,
    .release    = o_release,
    .fsync      = o_fsync,
    .opendir    = o_opendir,
    .readdir    = o_readdir,
    .releasedir = o_releasedir,
    .fsyncdir   = o_fsyncdir,
    .init       = o_init,
    .destroy    = o_destroy,
    .access     = o_access,
    .create     = o_create,
    .lock       = o_lock,
    .utimens    = o_utimens
};

struct options options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};


void show_help(const char *progname) {
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"hello\" file\n"
	       "                        (default: \"hello\")\n"
	       "    --contents=<s>      Contents \"hello\" file\n"
	       "                        (default \"Hello, World!\\n\")\n"
	       "\n");
}
