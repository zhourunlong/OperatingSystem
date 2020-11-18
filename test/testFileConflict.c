#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int cpid, flag, status;
  int fileDescriptor;
  char* openArgv[3]= {"testFileConflictChild", "o"};
  char* createArgv[3]= {"testFileConflictChild", "c"};
  
  printf("========== File operation conflict ==========\n");
  fileDescriptor = open("testFileConflict.file");
  printf("Open testFileConflict.file as #%d.\n", fileDescriptor);
  
  printf("Executing testFileConflictChild.coff for open conflict: expect return flag = -1.\n");
  cpid = exec("testFileConflictChild.coff", 2, openArgv);
  flag = join(cpid, &status);
  
  printf("Executing testFileConflictChild.coff for create conflict: expect return flag = -1.\n");
  cpid = exec("testFileConflictChild.coff", 2, createArgv);
  flag = join(cpid, &status);
  printf("========================================\n\n");
  printf("File operation conflict test successfully done.\n");
  
  return 0;
}
