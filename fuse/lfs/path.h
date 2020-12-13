#ifndef path_h
#define path_h

#include <string>

std::string relative_to_absolute(const char*, const char*, const int);
std::string resolve_prefix(const char*);
void generate_prefix(const char*);
std::string current_fname(const char*);
int locate(const char* path, int &i_number);

#endif
