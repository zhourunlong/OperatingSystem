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
        char buf[1001];
        int file_handle;
        
        sprintf(s, "%d.perm", i);
        file_handle = open(s, O_RDWR, 0777);
        pread(file_handle, buf, 1000, 0);
        close(file_handle);
        
        char ans[1001];
        sprintf(ans, "This is a permanent small file (%d.perm). You should be able to read this correctly.\n", i);
        
        if (strcmp(ans, buf) != 0) {
            printf("Wrong at file %d.perm: \'%s\'.\n", i, buf);
        }
    }
    return 0;
}
