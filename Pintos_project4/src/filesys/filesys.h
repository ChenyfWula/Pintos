#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
//@4-4 #include
#include "filesys/directory.h"
#include "filesys/inode.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
//@4-4 C: FC.d.ver 
bool filesys_create (const char *name, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

//@4-4 F: dir_parent_inode
struct inode *dir_parent_inode(struct dir* dir);
//@4-4 F: dir_parent_inode
struct inode *dir_parent_inumber(struct dir* dir);
//@4-4 F: path_to_name
char *path_to_name(const char* path_name);
//@4-4 F: move_dir
struct dir *move_dir(const char* path_name);
//@4-4 F: filesys_change_dir()
bool filesys_change_dir(const char *str_dir);

#endif /* filesys/filesys.h */
