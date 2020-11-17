#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"


int main(void) {
  int fileDescriptor, readCount, writeCount, closeFlag, deleteFlag;
  char content[9] = "Testing.";
  char buffer[9];
    
  printf("========== Unlinkage of an occupied file ==========\n");
  fileDescriptor = creat("testFileUnlink.file");
  printf("Create a new file testFileUnlink.file as #%d.\n", fileDescriptor);
  writeCount = write(fileDescriptor, content, 9);
  printf("Write %d characters to file #%d (testFileUnlink.file).\n", writeCount, fileDescriptor);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileUnlink.file #%d: flag = %d.\n", fileDescriptor, closeFlag);
  
  fileDescriptor = open("testFileUnlink.file");
  printf("Open testFileUnlink.file as file #%d.\n", fileDescriptor);
  readCount = read(fileDescriptor, buffer, 9);
  buffer[9] = 0;
  printf("Read %d characters from file #%d (testFileUnlink.file): \"%s\".\n", readCount, fileDescriptor, buffer);
  
  deleteFlag = unlink("testFileUnlink.file");
  printf("[CRITICAL] Mark testFileUnlink.file as unlinked, but do not delete at once: flag = %d.\n", deleteFlag);

  readCount = read(fileDescriptor, buffer, 9);
  buffer[9] = 0;
  printf("[CRITICAL] We may still read from file #%d (testFileUnlink.file): %d characters, \"%s\".\n", fileDescriptor, readCount, buffer);
  
  closeFlag = close(fileDescriptor);
  printf("[CRITICAL] Now we close file testFileUnlink.file to delete it: flag = %d.\n", closeFlag);
  
  fileDescriptor = open("testFileUnlink.file");
  printf("[CRITICAL] We shall not be able to open testFileUnlink.file: flag = %d.\n", fileDescriptor);
  printf("========================================\n\n");
  
  printf("File unlinking test successfully done.\n");
  return 0;
}
