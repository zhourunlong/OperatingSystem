#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int fileDescriptor1, fileDescriptor2, writeCount, readCount, closeFlag;
  char content1[9] = "Testing.";
  char content2[9] = "Rewrite.";
  char buffer[9];
  
  printf("========== Read an existing file ==========\n");
  fileDescriptor1 = open("testFileBasic0.file");
  printf("Open testFileBasic0.file as file #%d.\n", fileDescriptor1);
  readCount = read(fileDescriptor1, buffer, 9);
  buffer[9] = 0;
  printf("Read %d characters from file #%d (testFileBasic1.file): \"%s\".\n", readCount, fileDescriptor1, buffer);
  closeFlag = close(fileDescriptor1);
  printf("Close file #%d (testFileBasic1.file): flag = %d.\n", fileDescriptor1, closeFlag);
  printf("========================================\n\n");
  
  printf("========== Create a file and write (verify by reading) ==========\n");
  fileDescriptor1 = creat("testFileBasic1.file");
  printf("Create a new file testFileBasic1.file as #%d.\n", fileDescriptor1);
  writeCount = write(fileDescriptor1, content1, 9);
  printf("Write %d characters to file #%d (testFileBasic1.file).\n", writeCount, fileDescriptor1);
  closeFlag = close(fileDescriptor1);
  printf("Close file testFileBasic1.file #%d: flag = %d.\n", fileDescriptor1, closeFlag);

  fileDescriptor1 = open("testFileBasic1.file");
  printf("Open testFileBasic1.file as file #%d.\n", fileDescriptor1);
  readCount = read(fileDescriptor1, buffer, 9);
  printf("Read %d characters from file #%d (testFileBasic1.file): \"%s\".\n", readCount, fileDescriptor1, buffer);
  closeFlag = close(fileDescriptor1);
  printf("Close file #%d (testFileBasic1.file): flag = %d.\n", fileDescriptor1, closeFlag);
  printf("========================================\n\n");
  
  printf("========== Handle 2 files simultaneously ==========\n");
  fileDescriptor1 = creat("testFileBasic1.file");
  printf("Recreate the file testFileBasic1.file as #%d.\n", fileDescriptor1);
  writeCount = write(fileDescriptor1, content2, 9);
  printf("Write %d characters to file #%d (testFileBasic1.file).\n", writeCount, fileDescriptor1);
  fileDescriptor2 = creat("testFileBasic2.file");
  printf("Create a new file testFileBasic2.file as #%d.\n", fileDescriptor2);
  writeCount = write(fileDescriptor2, content2, 9);
  printf("Write %d characters to file #%d (testFileBasic1.file).\n", writeCount, fileDescriptor2);
  closeFlag = close(fileDescriptor1);
  printf("Close file #%d (testFileBasic2.file): flag = %d.\n", fileDescriptor1, closeFlag);
  closeFlag = close(fileDescriptor2);
  printf("Close file #%d (testFileBasic2.file): flag = %d.\n", fileDescriptor2, closeFlag);
  printf("========================================\n\n");
  
  printf("========== Verify an over-written file ==========\n");
  fileDescriptor1 = open("testFileBasic1.file");
  printf("Open testFileBasic1.file as file #%d.\n", fileDescriptor1);
  readCount = read(fileDescriptor1, buffer, 9);
  printf("Read %d characters from file #%d (recreated testFileBasic1.file): \"%s\".\n", readCount, fileDescriptor1, buffer);
  closeFlag = close(fileDescriptor1);
  printf("Close file #%d (testFileBasic1.file): flag = %d.\n", fileDescriptor1, closeFlag);
  printf("========================================\n\n");
  
  printf("========== Delete a file ==========\n");
  int deleteFlag = unlink("testFileBasic2.file");
  printf("Delete file testFileBasic2.file: flag = %d.\n", deleteFlag);
  printf("========================================\n\n");
  
  printf("Basic file operation test successfully done.\n");
  return 0;
}
