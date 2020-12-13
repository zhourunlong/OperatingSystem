#ifndef path_h
#define path_h

extern int mount_dir_len;

void relative_to_absolute(const char*, const char*, const int, char* nret);
void resolve_prefix(const char*, char* nret);
void generate_prefix(const char*);
void current_fname(const char*, char* ret);
int locate(const char* path, int &i_number);

#endif
