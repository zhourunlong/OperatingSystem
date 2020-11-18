#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  if (argc != 2) {
    printf("Wrong argument number.\n");
    return -1;
  }
  char op = argv[1][0];
  
  int fileDescriptor, closeFlag;
  if (op == 'o') {
      fileDescriptor = open("testFileConflict.file");
      printf("Open testFileConflict.file in child process: return flag = #%d.\n", fileDescriptor);
      closeFlag = close(fileDescriptor);
  } else if (op == 'c') {
      fileDescriptor = creat("testFileConflict.file");
      printf("Create testFileConflict.file in child process: return flag = #%d.\n", fileDescriptor);
      closeFlag = close(fileDescriptor);
  }

  return 0;
}
