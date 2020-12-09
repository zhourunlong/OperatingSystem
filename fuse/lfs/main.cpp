#include "index.h"
#include "logger.h"
#include "path.h"

int main(int argc, char** argv) {
    set_log_level(DEBUG);
    set_log_output(stdout);
    generate_prefix(argv[1]);

    int ret; 
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    ret = fuse_main(args.argc, args.argv, &ops, NULL);
	fuse_opt_free_args(&args);
	return ret;
}