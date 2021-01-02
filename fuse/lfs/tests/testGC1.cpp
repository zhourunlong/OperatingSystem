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
        sprintf(s, "%d", i);
        int file_handle = open(s, O_CREAT | O_RDWR, 0777);
        char* buf = (char*) malloc(1048576);
        memset(buf, 0, 1048576);
        pwrite(file_handle, buf, 1048575, 0);
        free(buf);
        close(file_handle);
        
        sprintf(s, "rm %d", i);
        system(s);
    }
    return 0;
}
