#include <bits/stdc++.h>
using namespace std;
int main(int argc, char *argv[]) {
    int n = atoi(argv[2]);
    for (int i = 0; i < n; ++i) {
        char s[99];
        sprintf(s, "mkdir ./%d\n", i);
        system(s);
    }
}
