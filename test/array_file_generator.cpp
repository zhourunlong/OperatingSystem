#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using namespace std;

const int n = 10;

int main() {
	freopen("array1.txt", "w", stdout);
	srand(time(0));
	for (int i = 1; i <= n; i++) {
		printf("%d ", rand());
	}
	printf("\n");
	return 0;
}