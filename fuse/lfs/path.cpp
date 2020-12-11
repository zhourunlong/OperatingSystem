#include "path.h"

#include "logger.h"
#include "utility.h"
#include "blockio.h"

#include <string.h>  /* strlen strcat strcpy */
#include <unistd.h>  /* getcwd */
#include <stdlib.h>  /* malloc free */
#include <stdio.h>
#include <string.h>  /* strcat strcpy */
#include <errno.h>   /* errno */
#include <vector>
#include <string>

#include <time.h>       /* clock_gettime */
#include <sys/stat.h>   /* struct timespec */

char* current_working_dir = NULL;
char* mount_root_dir = NULL;

const int SC = sizeof(char);

char* relative_to_absolute(const char* root, const char* path, const int bgi = 0) {
    int rlen = strlen(root), plen = strlen(path);

    char* ret = (char*) malloc((rlen + plen + 10) * SC);
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
                if (ERROR_PATH)
                    logger(ERROR, "[ERROR] Invalid relative path.");
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
    char* nret = (char*) malloc((rend + 1) * SC);
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
        current_working_dir = (char*) malloc(cwd_len * SC);
        errno = 0;
        getcwd(current_working_dir, cwd_len * SC);
    } while (errno == ERANGE);
    current_working_dir = relative_to_absolute(current_working_dir, "./");
    if (DEBUG_PATH) logger(DEBUG, "Current working dir =\t%s.\n", current_working_dir);
    
    mount_root_dir = relative_to_absolute(current_working_dir, path);
    if (DEBUG_PATH) logger(DEBUG, "Mount root dir =\t%s.\n", mount_root_dir);
}

char* current_fname(const char* path) {
    int plen = strlen(path);
    while (plen && path[plen - 1] == '/')
        --plen;
    if (!plen) {
        if (ERROR_PATH)
            logger(ERROR, "[ERROR] Current file is root directory.");
        return NULL;
    }
    int i = plen - 1;
    while (i && path[i] != '/')
        --i;
    char* ret = (char*) malloc((plen - i) * SC);
    memcpy(ret, path + i + 1, (plen - i - 1) * SC);
    ret[plen - i - 1] = 0;
    return ret;
}

/** Traverse an absolute path to retrieve inode number.
 * @param  _path: an absolute path from LFS root.
 * @param  i_number: return variable.
 * @return flag: indicating whether the traversal is successful. */
int locate(const char* _path, int &i_number) {
    if (_path[0] != '/') {
        if (ERROR_PATH)
            logger(ERROR, "[ERROR] Function locate() only accepts absolute path from LFS root.\n");
        return -EPERM;
    }

    /* Generate report: for debugging only. */
    if (DEBUG_LOCATE_REPORT)
        logger(DEBUG, "\n[DEBUG] ******************** PRINT LOCATE() REPORT ********************\n");

    // Split path.
    std::string path = _path;
    path += "/";
    std::string temp_str;
    std::vector<std::string> split_path;
    int start_pos = 1;
    for (int i=1; i<path.length(); i++) {
        if (path[i] == '/') {
            temp_str = path.substr(start_pos, i-start_pos);
            i++;
            start_pos = i;

            if ((temp_str == ".") || (temp_str == "")) {
                continue;
            } else if (temp_str == "..") {
                split_path.pop_back();
            } else {
                split_path.push_back(temp_str);
            }
        }
    }

    // Retrive system time (for atime updates).
    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    // Traverse split path from LFS root directory.
    struct inode block_inode;
    directory block_dir;
    bool flag;
    std::string target;

    int cur_inumber = ROOT_DIR_INUMBER;
    for (int d=0; d<split_path.size(); d++) {
        get_inode_from_inum(&block_inode, cur_inumber);
        if (FUNC_ATIME_DIR) {
            update_atime(block_inode, cur_time);  // Update atime according to FUNC_ATIME_ flags.
            new_inode_block(&block_inode);
        }
        
        target = split_path[d];
        if (DEBUG_LOCATE_REPORT) {
            logger(DEBUG, "Access #%d: target = %s ...\n", d, target.c_str());
            logger(DEBUG, "-- Searching inode #%d.\n", cur_inumber);
        }

        if (block_inode.mode != MODE_DIR) {
            if (DEBUG_LOCATE_REPORT) {
                logger(DEBUG, "-x Failed to read the inode: %s is not a directory. Procedure aborted.\n", target.c_str());
                logger(DEBUG, "============================ PRINT LOCATE() REPORT ====================\n\n");
            } else if (ERROR_PATH) {
                logger(ERROR, "[ERROR] %s is not a directory.\n", target.c_str());
            }
            return -ENOTDIR;
        }

        flag = false;
        while (!flag) {
            for (int i=0; i<NUM_INODE_DIRECT; i++) {
                if (block_inode.direct[i] <= -1)
                    continue;
                if (block_inode.direct[i] > FILE_SIZE) {
                    if (block_inode.num_direct > i) {
                        logger(ERROR, "[FATAL ERROR] Corrupt file system on disk: invalid direct[%d] of inode #%d.\n", i, block_inode.i_number);
                        exit(-1);
                    }
                    if (ERROR_PATH) logger(ERROR, "[ERROR] Inode not correctly initialized: invalid direct[%d] of inode #%d.\n", i, block_inode.i_number);
                    continue;
                }
                get_block(block_dir, block_inode.direct[i]);

                for (int j=0; j<MAX_DIR_ENTRIES; j++) {
                    if (block_dir[j].i_number <= 0)
                        continue;
                    if ((block_dir[j].i_number > MAX_NUM_INODE) && ERROR_PATH)
                        logger(ERROR, "[ERROR] Inode not correctly initialized: invalid i_number #%d.\n", block_dir[j].i_number);
                    
                    if (block_dir[j].filename == target) {
                        cur_inumber = block_dir[j].i_number;
                        flag = true;
                        break;
                    }
                }

                if (flag) break;
            }
            if (flag) break;

            if (block_inode.next_indirect == 0) break;

            get_inode_from_inum(&block_inode, block_inode.next_indirect);
            if (DEBUG_LOCATE_REPORT)
                logger(DEBUG, "-- Searching (non-head) inode #%d.\n", block_inode.next_indirect);
        }

        if (DEBUG_LOCATE_REPORT)
            if (flag) {
                logger(DEBUG, "=> Successfully retrieved next-level inode entry: '%s' ====> #%d.\n\n", target.c_str(), cur_inumber);
            } else {
                logger(DEBUG, "-x Failed to retrieve next-level inode entry. Procedure aborted.\n");
                logger(DEBUG, "============================ PRINT LOCATE() REPORT ====================\n\n");
            }

        if (!flag)
            return -ENOENT;
    }
    i_number = cur_inumber;

    /* Generate report: for debugging only. */
    if (DEBUG_LOCATE_REPORT) {
        logger(DEBUG, ">>> traversal succeeded <<<\n");
        logger(DEBUG, "PATH = %s\n", _path);
        logger(DEBUG, "I_NUMBER = %d\n", i_number);
        logger(DEBUG, "INODE_BLOCK_ADDR = %d\n", inode_table[i_number]);

        logger(DEBUG, "INODE INFORMATION:\n");
        struct inode* node = (struct inode*) malloc(sizeof(struct inode));
        get_inode_from_inum(node, i_number);
        print(node);
        free(node);

        logger(DEBUG, "============================ PRINT LOCATE() REPORT ====================\n\n");
    }

    return 0;
}