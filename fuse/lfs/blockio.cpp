#include "unistd.h"
#include "utility.h"
#include "stdio.h"
#include <vector>
#include <string>

// Must take an absolute path (from LFS root).
int locate(char* _path, int &i_number) {
    if (_path[0] != "/") {
        logger(ERROR, "Function locate() only accepts absolute path from LFS root.\n");
        return -1;
    }

    // Split path.
    std::string path = _path;
    std::string temp_str;
    std::vector<std::string> split_path;
    int start_pos = 1;
    for (int i=1; i<path.length(); i++) {
        if (path[i] == "/") {
            temp_str = path.substr(start_pos, i-start_pos);
            i++;
            start_pos = i;

            if (temp_str == ".") {
                continue;
            } else if (temp_str == "..") {
                split_path.pop_back();
            } else {
                split_path.push_back(temp_str);
            }
        }
    }

    // Traverse split path from LFS root directory.
    int cur_inumber = root_dir_inumber;
    inode block_inode;
    directory block_dir;
    bool flag;
    std::string target;
    for (int d=0; d<split_path.size(); d++) {
        read_block(block_inode, cur_inumber);
        target = split_path[d];
        flag = true;
        while (flag) {
            for (int i=0; i<NUM_INODE_DIRECT; i++) {
                if (block_inode.direct[i] == 0)
                    continue;
                read_block(block_dir, block_inode.direct[i]);

                for (int j=0; j<MAX_DIR_ENTRIES; j++) {
                    if (block_dir.dir_entry[j].i_number <= 0)
                        continue;
                    if (block_dir.dir_entry[j].filename == target) {
                        cur_inumber = block_dir.dir_entry[j].i_number;
                        flag = false;
                        break;
                    }
                }

                if (flag) break;
            }
            if (flag) break;

            if (block_inode.next_indirect == 0)
                break;
            read_block(block_inode, block_inode.next_indirect);
        }

        if (!flag)
            return -2;  // File or directory does not exist.
    }

    i_number = cur_inumber;
    return 0;
}

int get_block(void* data, int block_addr) {
    int segment = block_addr / BLOCKS_IN_SEGMENT;
    int block = block_addr % BLOCKS_IN_SEGMENT;

    if (segment == cur_segment) {    // Data in segment buffer.
        int buffer_offset = block * BLOCK_SIZE;
        memcpy(data, segment_buffer + buffer_offset, BLOCKSIZE);
    } else {    // Data in disk file.
        read_block(data, block_addr);
    }
    return 0;
}

int new_block(void* data) {
    int buffer_offset = cur_block * BLOCK_SIZE;
    memcpy(segment_buffer + buffer_offset, data, BLOCK_SIZE);
    
    if (cur_block == BLOCKS_IN_SEGMENT) {    // Segment buffer is full, and should be flushed to disk file.
        write_segment(segment_buffer, cur_segment);
        cur_segment++;
        cur_block = 0;
    } else {    // Segment buffer is not full yet.
        cur_block++;
    }
    return 0;
}