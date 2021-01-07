#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
const int N = 1000;
int main() {
    for (int i = 0; i < N; ++i) {
        char s[999];
        
        char* buf = (char*) malloc(1048576);
        memset(buf, 0, 1048576);
        sprintf(buf, "This is a to-be-deleted large file (%d). You are not supposed to be able to read this.\n", i);
        
        sprintf(s, "%d", i);
        int file_handle = open(s, O_CREAT | O_RDWR, 0777);
        pwrite(file_handle, buf, 1048575, 0);
        close(file_handle);
        
        free(buf);
        
        sprintf(s, "rm %d", i);
        system(s);
    }
    return 0;
}
