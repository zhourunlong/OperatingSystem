#ifndef path_h
#define path_h

char* relative_to_absolute(const char*, const char*, const int);
char* resolve_prefix(const char*);
void generate_prefix(const char*);
char* current_fname(const char*);
int locate(const char* path, int &i_number);

#endif