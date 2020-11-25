#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int fileDescriptor, closeFlag, deleteFlag, readCount, writeCount;
  char readBuffer[10];
  char writeBuffer[10] = "Success!!";
  
  printf("========== Deal with non-existing files ==========\n");
  fileDescriptor = open("nonexisting.file");
  printf("When trying to open nonexisting.file, return descriptor %d.\n", fileDescriptor);
  deleteFlag = unlink("nonexisting.file");
  printf("When trying to delete nonexisting.file, return flag = %d.\n", deleteFlag);
  printf("========================================\n\n");
  
  printf("========== Deal with free file descriptors ==========\n");
  readCount = read(16, readBuffer, 10);
  printf("When trying to read a free file descriptor, return flag = %d.\n", readCount);
  writeCount = write(16, writeBuffer, 10);
  printf("When trying to write a free file descriptor, return flag = %d.\n", writeCount);
  printf("========================================\n\n");
  
  printf("========== Deal with buffer overflows ==========\n");
  fileDescriptor = open("testFileError0.file");
  printf("Open testFileError0.file as file #%d.\n", fileDescriptor);
  readCount = read(fileDescriptor, readBuffer, 20);
  printf("When read buffer overflows, return read count = %d. Buffer contents = %s.\n", readCount, readBuffer);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileError0.file: flag = %d.\n", closeFlag);
  
  fileDescriptor = creat("testFileError1.file");
  printf("Create testFileError1.file as file #%d.\n", fileDescriptor);
  writeCount = write(fileDescriptor, writeBuffer, 20);
  printf("When write buffer overflows, return write count = %d. Buffer contents = %s.\n", writeCount, writeBuffer);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileError1.file: flag = %d.\n", closeFlag);
  
  fileDescriptor = creat("testFileError2.file");
  printf("Create testFileError2.file as file #%d.\n", fileDescriptor);
  writeCount = write(fileDescriptor, (char*) 0, 16*1024);
  printf("When write buffer = total memory capacity, return write count = %d.\n", writeCount);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileError2.file: flag = %d.\n", closeFlag);
  printf("========================================\n\n");
  
  printf("========== Deal with wrong buffer length ==========\n");
  fileDescriptor = open("testFileError0.file");
  printf("Open testFileError0.file as file #%d.\n", fileDescriptor);
  readCount = read(fileDescriptor, readBuffer, 0);
  printf("When read length = 0, return read count = %d.\n", readCount);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileError0.file: flag = %d.\n", closeFlag);
  
  fileDescriptor = creat("testFileError3.file");
  printf("Create testFileError3.file as file #%d.\n", fileDescriptor);
  writeCount = write(fileDescriptor, writeBuffer, 0);
  printf("When write length = 0, return write count = %d.\n", writeCount);
  closeFlag = close(fileDescriptor);
  printf("Close file testFileError3.file: flag = %d.\n", closeFlag);
  printf("========================================\n\n");
  
  printf("========== Try to close stdin & stdout ==========\n");
  closeFlag = close(0);
  printf("Close stdin: flag = %d.\n", closeFlag);
  printf("We will close stdout soon. No more displaying on console.\n");
  closeFlag = close(1);
  printf("Close stdout: flag = %d.\n", closeFlag);
  printf("========================================\n\n");
  
  printf("Invalid file operation test successfully done.\n");
  return 0;
}
