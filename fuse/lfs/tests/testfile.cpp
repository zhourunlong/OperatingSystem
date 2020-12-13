#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int file_handle = open("../test/test.txt", O_CREAT | O_RDWR, 0777);
    char* buf = (char*) malloc(100);
    memset(buf, 0, 100);
    pwrite(file_handle, buf, 100, 0);
    free(buf);
    close(file_handle);
    return 0;
}