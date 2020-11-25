#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

char buffer[10000];
int main(int argc, char** argv) {
  int fileDescriptor, readCount, closeFlag;
  char content1[9], content2[9];
  
  printf("Open an array file (buffer in static section - no overflow problem).\n");
  fileDescriptor = open("testMemoryArray.txt");
  printf("Open testMemoryArray.txt as file #%d.\n", fileDescriptor);
  readCount = read(fileDescriptor, buffer, 10000);
  printf("readCount = %d.\n", readCount);
  printf("========================================\n\n");
  
  return 0;
}
