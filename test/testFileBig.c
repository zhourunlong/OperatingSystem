#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"


int main(void) {
  int fileDescriptor, readCount, writeCount, closeFlag, deleteFlag;
  char writeBuffer[2048], readBuffer[2048];
  const int SIZE = 2048;
  int i;
  
  printf("========== Writing into a big file ==========\n");
  for (i = 0; i < SIZE; i++)
      writeBuffer[i] = 'a' + (i*i % 26);
  writeBuffer[SIZE-1] = 0;

  fileDescriptor = creat("testFileBig.file");
  printf("Create a new file testFileBig.file as #%d.\n", fileDescriptor);
  writeCount = write(fileDescriptor, writeBuffer, SIZE);
  printf("Write %d characters to file #%d (testFileBig.file).\n", writeCount, fileDescriptor);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileBig.file #%d: flag = %d.\n", fileDescriptor, closeFlag);
  printf("========================================\n\n");
      
  printf("========== Reading from a big file ==========\n");
  fileDescriptor = open("testFileBig.file");
  printf("Open testFileBig.file as #%d.\n", fileDescriptor);
  readCount = read(fileDescriptor, readBuffer, SIZE);
  printf("Read %d characters from file #%d (testFileBig.file).\n", readCount, fileDescriptor);

  for (i = 0; i < SIZE; i++) {
    if (readBuffer[i] != writeBuffer[i]) {
      printf("[ERROR] Retrieve invalid data in reading.");
      break;
    }
  }
  
  closeFlag = close(fileDescriptor);
  printf("Close file testFileBig.file to delete it: flag = %d.\n", closeFlag);
  deleteFlag = unlink("testFileBig.file");
  printf("Delete file testFileBig.file: flag = %d.\n", deleteFlag);
  printf("========================================\n\n");
  
  printf("Big file operation test successfully done.\n");
  return 0;
}
