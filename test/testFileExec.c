#include "syscall.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
  int* status;
  *status = -1;
  int cpid = exec("testFileMultiple.coff", argc, argv);
  int flag = join(cpid, status);
  return *status;
}
