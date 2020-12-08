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
    int cur_inode = root_dir_inode;
    


    return 0;
}

int get_block(int block_addr, void* data){
    return 0;
}

int new_block(void* data){
    return 0;
}