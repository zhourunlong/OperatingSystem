#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  if (argc != 2) {
      printf("Wrong argument number.\n");
      return -1;
  }
  
  int r = atoi(argv[1]);
  int status = -1;
  int cpid;
  
  if (r == 0) {
      sprintf(argv[0], "testFileMultiple");
      printf("Executing testFileMultiple.coff.\n");
      cpid = exec("testFileMultiple.coff", 1, argv);
  } else {
      sprintf(argv[1], "%d", r-1);
      printf("Executing next-level recursion with parameter %s.\n", argv[1]);
      cpid = exec("testExecNesting.coff", argc, argv);
  }
  int flag = join(cpid, &status);
  return status;
}
