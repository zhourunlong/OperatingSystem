// Consts.
const int MAX_DIR_ENTRY = ???;
int BLOCK_SIZE, TOT_INODES, TOT_BLOCKS;
int SEGMENT_SIZE, BLOCK_SIZE, MAX_FILENAME;

// Physical structure.
struct superblock {
    pass;
}

struct block {
    pass;
}

// Logical structure.
struct inode {
    pass;
}

struct indirect {
    pass;
}

struct directory {
    pass;
}

struct inode_map {
    pass;
}

struct segment {
    pass;
}

struct checkpoint {
    pass;
}

int read_log();
int write_log();

char segment_buffer[];
int fd;
int count_inode, cur_segment_no, cur_block_no;