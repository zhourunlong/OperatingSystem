#include "path.h"

#include "logger.h"

#include <string.h>  /* strlen strcat strcpy */
#include <unistd.h>  /* getcwd */
#include <stdlib.h>  /* malloc free */
#include <string.h>  /* strcat strcpy */
#include <errno.h>   /* errno */
#include <stdio.h>
#include <vector>
#include <string>

char* current_working_dir = NULL;
char* mount_root_dir = NULL;

const int SC = sizeof(char);

char* relative_to_absolute(const char* root, const char* path, const int bgi = 0) {
    int rlen = strlen(root), plen = strlen(path);

    char* ret = (char *)malloc((rlen + plen + 10) * SC);
    memcpy(ret, root, rlen * SC);
    ret[rlen] = 0;
    int rend = rlen;
    if (ret[rend - 1] != '/')
        ret[rend++] = '/';
    for (int i = bgi; i < plen;) {
        if (i + 1 < plen && path[i] == '.' && path[i + 1] == '/') {
            i += 2;
            continue;
        }
        if (i + 2 < plen && path[i] == '.' && path[i + 1] == '.' && path[i + 2] == '/') {
            i += 3;
            if (rend <= 1) {
                logger(ERROR, "invalid relative path!");
                continue;
            }
            for (rend -= 2; rend >= 0 && ret[rend] != '/'; --rend);
            ++rend;
            continue;
        }
        for (int j = i; j < plen && path[j] != '/'; ++j) {
            ret[rend++] = path[j];
            i = j;
        }
        ret[rend++] = '/';
        i += 2;
    }
    if (path[plen - 1] != '/')
        --rend;
    char* nret = (char *)malloc((rend + 1)* SC);
    memcpy(nret, ret, rend * SC);
    nret[rend] = 0;
    return nret;
}

char* resolve_prefix(const char* path) {
    return relative_to_absolute(mount_root_dir, path, path[0] == '/');
}

void generate_prefix(const char* path) {
    int cwd_len = 0;
    do {
        cwd_len += 10;
        if (current_working_dir != NULL)
            free(current_working_dir);
        current_working_dir = (char *)malloc(cwd_len * SC);
        errno = 0;
        getcwd(current_working_dir, cwd_len * SC);
    } while (errno == ERANGE);
    current_working_dir = relative_to_absolute(current_working_dir, "./");
    logger(DEBUG, "current working dir =\t%s\n", current_working_dir);
    
    mount_root_dir = relative_to_absolute(current_working_dir, path);
    logger(DEBUG, "mount root dir =\t%s\n", mount_root_dir);
}


/** Traverse an absolute path to retrieve inode number.
 * @param  _path: an absolute path from LFS root.
 * @param  i_number: return variable.
 * @return flag: indicating whether the traversal is successful. */
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