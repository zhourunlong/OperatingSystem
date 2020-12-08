#include "index.h"

int main(int argc, char** argv) {
    set_log_level(DEBUG);
    set_log_output(stdout);
    generate_prefix(argv[1]);

    int ret; 
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    extern struct options options;
    
    options.filename = strdup("hello");
	options.contents = strdup("Hello World!\n");

    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

    if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

    ret = fuse_main(args.argc, args.argv, &ops, NULL);
	fuse_opt_free_args(&args);
	return ret;
}