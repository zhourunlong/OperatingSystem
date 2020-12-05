#include "./prefix.h" /* resolve_prefix generate_prefix */
#include "./init.h" /* o_init */
#include "./getattr.h" /* o_getattr */
#include "./access.h" /* o_access */
#include "./readdir.h" /* o_readdir */
#include "./destroy.h" /* o_destroy */
#include "./opendir.h" /* o_opendir */
#include "./mkdir.h" /* o_mkdir */
#include "./unlink.h" /* o_unlink */
#include "./rmdir.h" /* o_rmdir */
#include "./rename.h" /* o_rename */
#include "./chmod.h" /* o_chmod */
#include "./chown.h" /* o_chown */
#include "./truncate.h" /* o_truncate */
#include "./utimens.h" /* o_utimens */
#include "./open.h" /* o_open */
#include "./read.h" /* o_read */
#include "./write.h" /* o_write */
#include "./statfs.h" /* o_statfs */
#include "./release.h" /* o_release */
#include "./releasedir.h" /* o_releasedir */
#include "./fsync.h" /* o_fsync */
#include "./fsyncdir.h" /* o_fsyncdir */
#include "./flush.h" /* o_flush */
#include "./lock.h" /* o_lock */

#include <fuse.h> /* fuse_main */

#include "./logger.h" /* set_log_level set_log_output logger */

static struct fuse_operations ops = {
    .getattr = o_getattr,
    .mkdir = o_mkdir,
    .unlink = o_unlink,
    .rmdir = o_rmdir,
    .rename = o_rename,
    .chmod = o_chmod,
    .chown = o_chown,
    .truncate = o_truncate,
    .open = o_open,
    .read = o_read,
    .write = o_write,
    .statfs = o_statfs,
    .flush = o_flush,
    .release = o_release,
    .fsync = o_fsync,
    .opendir = o_opendir,
    .readdir = o_readdir,
    .releasedir = o_releasedir,
    .fsyncdir = o_fsyncdir,
    .init = o_init,
    .destroy = o_destroy,
    .access = o_access,
    .lock = o_lock,
    .utimens = o_utimens,
};

int main(int argc, char** argv) {
    set_log_level(DEBUG);
    set_log_output(stdout);
    generate_prefix(argv[1]);

    return fuse_main(argc, argv, &ops, NULL);
}
