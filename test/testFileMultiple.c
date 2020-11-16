#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int fileDescriptor[16];
  int i, closeFlag, readCount, writeCount;
  char content1[9] = "Testing.";
  char content2[9] = "Rewrite.";
  char buffer[9];
  
  printf("========== Testing multiple-file operations ==========\n");
  for (i=0; i<16; i++) {
      sprintf(buffer, "%d.file", i);
      fileDescriptor[i] = creat(buffer);
      printf("Create a file %s and open it as file #%d.\n", buffer, fileDescriptor[i]);
  }
  
  for (i=0; i<14; i++) {
      writeCount = write(fileDescriptor[i], content1, 9);
      printf("Write %d characters to file #%d.\n", writeCount, fileDescriptor[i]);
      closeFlag = close(fileDescriptor[i]);
      printf("Close file #%d: flag = %d.\n", fileDescriptor[i], closeFlag);
  }
  
  for (i=0; i<7; i++) {
      sprintf(buffer, "%d.file", i);
      fileDescriptor[i] = creat(buffer);
      printf("Recreate a file %s and open it as file #%d.\n", buffer, fileDescriptor[i]);
      writeCount = write(fileDescriptor[i], content2, 9);
      printf("Write %d characters to file #%d.\n", writeCount, fileDescriptor[i]);
      closeFlag = close(fileDescriptor[i]);
      printf("Close file #%d: flag = %d.\n", fileDescriptor[i], closeFlag);
  }
  
  for (i=0; i<14; i++) {
      sprintf(buffer, "%d.file", i);
      fileDescriptor[i] = open(buffer);
      printf("Open %s as file #%d.\n", buffer, fileDescriptor[i]);
      readCount = read(fileDescriptor[i], buffer, 9);
      printf("Read %d characters from file #%d: \"%s\".\n", readCount, fileDescriptor[i], buffer);
      closeFlag = close(fileDescriptor[i]);
      printf("Close file #%d: flag = %d.\n", fileDescriptor[i], closeFlag);
  }
  printf("========================================\n");
  
  printf("Basic file operation test successfully done.\n");
  return 0;
}
