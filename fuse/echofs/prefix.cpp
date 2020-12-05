#include "prefix.h" /* prefix directory cwd */

char* current_working_dir = NULL;
char* mount_root_dir = NULL;

const int SC = sizeof(char);

char* relative_to_absolute(const char* root, const char* path) {
    int rlen = strlen(root), plen = strlen(path);
    char* ret = (char *)malloc((rlen + plen + 10) * SC);
    memcpy(ret, root, rlen * SC);
    int rend = rlen;
    if (ret[rend - 1] != '/')
        ret[rend++] = '/';
    for (int i = 0; i < plen;) {
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
    char* nret = (char *)malloc(rend * SC);
    memcpy(nret, ret, rend * SC);
    return nret;
}

char* resolve_prefix(const char* path) {
    return relative_to_absolute(mount_root_dir, path);
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
