#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
using namespace std;
const int TOT = 10000;
void func(int idx, int m) {
    for (int i = 0; i < m; ++i) {
        char s[999];
        sprintf(s, "mkdir ./%d\n", idx * m + i);
        system(s);
    }
    for (int i = 0; i < m; ++i) {
        int j = idx * m + i;
        char s[999];
        sprintf(s, "./%d/%d", j, j);
        int file_handle = open(s, O_CREAT | O_RDWR, 0777);
        char* buf = (char*) malloc(10000);
        memset(buf, 0, 10000);
        pwrite(file_handle, buf, 9999, 0);
        free(buf);
        close(file_handle);
    }
}
int main(int argc, char* argv[]) {
    int n = atoi(argv[1]), m = atoi(argv[2]);
    assert(1ll * n * m <= TOT);
    std::thread th[n];
    for (int i = 0; i < n; ++i) {
        th[i] = std::thread(func, i, m);
    }
    for (int i = 0; i < n; ++i) {
        th[i].join();
    }
    return 0;
}
