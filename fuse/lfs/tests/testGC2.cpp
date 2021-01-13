#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
const int N = 10000;
int main() {
    for (int i = 0; i < N; ++i) {
        char s[999];
        char* buf;
        int file_handle;
        
        buf = (char*) malloc(1000001);
        memset(buf, 0, 1000001);
        sprintf(buf, "This is a to-be-deleted large file (%d.del). You are not supposed to be able to read this.\n", i);
        sprintf(s, "%d.del", i);
        file_handle = open(s, O_CREAT | O_RDWR, 0777);
        pwrite(file_handle, buf, 1000000, 0);
        close(file_handle);
        free(buf);
        
        buf = (char*) malloc(1001);
        memset(buf, 0, 1001);
        sprintf(buf, "This is a permanent small file (%d.perm). You should be able to read this correctly.\n", i);
        sprintf(s, "%d.perm", i);
        file_handle = open(s, O_CREAT | O_RDWR, 0777);
        pwrite(file_handle, buf, 1000, 0);
        close(file_handle);
        free(buf);
        
        sprintf(s, "rm %d.del", i);
        system(s);
    }
    return 0;
}
