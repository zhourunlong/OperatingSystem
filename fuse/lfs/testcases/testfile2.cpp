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
        sprintf(s, "mkdir ../test/%d\n", i);
        system(s);
    }
    for (int i = 0; i < N; ++i) {
        char s[999];
        //sprintf(s, "touch ../test/%d/%d\n", i, i);
        //system(s);
        sprintf(s, "../test/%d/%d", i, i);
        int file_handle = open(s, O_CREAT | O_RDWR, 0777);
        char* buf = (char*) malloc(10000);
        memset(buf, 0, 10000);
        pwrite(file_handle, buf, 9999, 0);
        free(buf);
        close(file_handle);
    }
    return 0;
}
