#include <bits/stdc++.h>
using namespace std;
int main() {
    for (int i = 0; i < 10000; ++i) {
        char s[999];
        sprintf(s, "mkdir ../test/%d\n", i);
        system(s);
    }
}
