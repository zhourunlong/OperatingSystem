#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int fileDescriptor1, fileDescriptor2, writeCount, readCount, closeFlag;
  char content1[9] = "Testing.";
  char content2[9] = "Rewrite.";
  char buffer[1005];
  
  printf("========== Read array file ==========\n");
  fileDescriptor1 = open("array1.txt");

  printf("Open array1.txt as file #%d.\n", fileDescriptor1);
  readCount = read(fileDescriptor1, buffer, 10000);
  printf("%d\n", readCount);
  printf("%s\n", buffer);
  printf("========================================\n\n");
  
  printf("Test successfully done.\n");
  return 0;
}
