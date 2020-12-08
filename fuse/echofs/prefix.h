#ifndef prefix_h
#define prefix_h
#include "logger.h" /* logger */
#include <string.h> /* strlen strcat strcpy */
#include <unistd.h> /* getcwd */
#include <stdlib.h> /* malloc free */
#include <string.h> /* strcat strcpy */
#include <errno.h> /* errno */

char* relative_to_absolute(const char*, const char*, const int);
char* resolve_prefix(const char*);
void generate_prefix(const char*);
#endif
