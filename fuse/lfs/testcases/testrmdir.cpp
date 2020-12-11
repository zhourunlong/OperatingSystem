#include <bits/stdc++.h>
using namespace std;
const int N = 5000;
int f[N];
set <int> g[N];
vector <int> stk, avail;
void dfs(int u) {
    stk.push_back(u);
    string t = "";
    for (int i = 0; i < stk.size(); ++i)
        t += "/" + to_string(stk[i]);
    char s[999];
    sprintf(s, "mkdir ../zrl%s", t.c_str());
    system(s);
    for (set <int> :: iterator itr = g[u].begin(); itr != g[u].end(); ++itr)
        dfs(*itr);
    stk.pop_back();
}
int main() {
    srand(time(NULL));
    for (int i = 2; i < N; ++i) {
        f[i] = rand() % (i - 1) + 1;
        g[f[i]].insert(i);
    }
    dfs(1);
    for (int i = 1; i < N; ++i)
        avail.push_back(i);
    while (1) {
        int z = rand() % avail.size();
        int x = avail[z];
        stk.clear();
        int y = x;
        while (y) {
            stk.push_back(y);
            y = f[y];
        }
        reverse(stk.begin(), stk.end());
        string t = "";
        for (int i = 0; i < stk.size(); ++i)
            t += "/" + to_string(stk[i]);
        char s[999];
        sprintf(s, "rmdir ../zrl%s\n", t.c_str());
        int ret = system(s);
        //fprintf(stderr, "%s", s);
        int correct = 1;
        if (g[x].size() == 0)
            correct = 0;
        assert((correct == 0 && ret == 0) || (correct == 1 && ret != 0));
        if (g[x].size() == 0) {
            if (x == 1) return 0;
            //fprintf(stderr, "%d %d %d\n", x, f[x], g[f[x]].size());
            g[f[x]].erase(g[f[x]].find(x));
            avail.erase(avail.begin() + z);
        }
    }
}
