#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
const int N = 453;
int main() {
    for (int i = 0; i < N; ++i) {
        char s[999];
        char* buf;
        int file_handle;
        
        buf = (char*) malloc(1001);
        memset(buf, 0, 1001);
        sprintf(buf, "This is a permanent small file (%d.perm). You should be able to read this correctly.\n", i);
        sprintf(s, "%d.perm", i);
        file_handle = open(s, O_CREAT | O_RDWR, 0777);
        pwrite(file_handle, buf, 1000, 0);
        close(file_handle);
        free(buf);
    }
    return 0;
}
