#ifndef dir
#define dir

#include <fuse.h>
#include <sys/stat.h>

int o_opendir(const char*, struct fuse_file_info*);
int o_releasedir(const char*, struct fuse_file_info*);
int o_readdir(const char*, void*, fuse_fill_dir_t, off_t, struct fuse_file_info*, enum fuse_readdir_flags);
int o_mkdir(const char*, mode_t);
int o_rmdir(const char*);

// Universal utility functions for file and directory operations.
int append_parent_dir_entry(struct inode* head_inode, const char* new_name, int new_inum);
bool remove_parent_dir_entry(struct inode* block_inode, int del_inum);
bool remove_parent_dir_entry(struct inode* block_inode, int del_inum, const char* del_name);
int remove_object(struct inode* head_inode, const char* del_name, int del_mode);

#endif