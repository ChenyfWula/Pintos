#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
//@4-1 #include
#include "filesys/cache.h"
//@4-4 #include
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init (); //#A init a list open_node only
  free_map_init ();//initialize the free_map, which is a bit_map. Set the 0 and 1 to be true.

  if (format) 
    do_format (); //#A clear the bitmap in disk

  free_map_open (); //#A read free_map from Disk
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  //@4-1 flush
  flush_cache();

  free_map_close (); //@4-x for now only free_map written back
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{ //@4-4 C: FC.d.ver
  //@4-4 in: filesys_create.1
  struct dir *dir = move_dir(name); //#A return NULL, if wrong; one dir no-existed
  char *file_name = path_to_name(name); //#A file_name need free

  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();//#A root-dir init in do_format, Original
  bool success = false;
  if (strcmp(file_name, ".") != 0 && strcmp(file_name, "..") != 0){
    success = (dir != NULL
                && free_map_allocate (1, &inode_sector)
    && inode_create(inode_sector, initial_size, is_dir, dir_parent_inumber(dir))
                && dir_add (dir, file_name, inode_sector)); //#A check name inside
  } //@4-4 C: IC.d.ver
  
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  //@4-4 in: filesys_create.2
  free(file_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //@4-4 in: filesys_open
  if(strlen(name) == 0)
    return NULL;

  struct dir *dir = move_dir(name); //#A return NULL, if wrong
  char *file_name = path_to_name(name); //#A file_name need free

  //struct dir *dir = dir_open_root (); Original
  struct inode *inode = NULL;
  
  if (dir != NULL){
    if (strcmp(file_name, "..") == 0){
      inode = dir_parent_inode(dir);
      if (!inode){
  	    free(file_name);
  	    return NULL;
  	  }
    }
    else if ((dir_is_root(dir) && strlen(file_name) == 0) ||
	            strcmp(file_name, ".") == 0){ //#A '/' in end
  	  free(file_name);
      struct file *file = file_open (inode_reopen (dir_get_inode (dir)));
      dir_close(dir);
      return file;
  	  //return (struct file *) dir;
  	}
    else
  	  dir_lookup (dir, file_name, &inode); //#A inode(new-created) point to MEM
  }
  dir_close (dir);
  free(file_name);
  if (!inode)
    return NULL;
  if (inode_is_dir(inode))
    return (struct file *) dir_open(inode);
  
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //@4-4 in: filesys_remove.1
  struct dir *dir = move_dir(name); //#A return NULL, if wrong
  char *file_name = path_to_name(name); //#A file_name need free
  //struct dir *dir = dir_open_root ();

  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 
  //@4-4 in: filesys_remove.2
  free(file_name);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create (); //#A create free_map_file, free_map init in filesys_init
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close (); //#A free_map_file = NULL
  printf ("done.\n");
}

//@4-4 F: dir_parent_inode
struct inode* dir_parent_inode(struct dir* dir){
  if(dir == NULL)  
    return NULL;
  block_sector_t sector = inode_get_dir_in(dir_get_inode(dir)); //#A 1 if root
  return inode_open(sector);
}
//@4-4 F: dir_parent_inumber
struct inode *dir_parent_inumber(struct dir* dir){
  if(dir == NULL)  
    return NULL;
  return inode_get_inumber(dir_get_inode(dir));
}

//@4-4 F: path_to_name, we move to last '/', even at the end
char* path_to_name(const char* path_name){
  int length = strlen(path_name);  
  char path[length + 1];
  memcpy(path, path_name, length + 1);

  char *cur, *ptr, *prev = "";
  for(cur = strtok_r(path, "/", &ptr); cur != NULL; 
      cur = strtok_r(NULL, "/", &ptr))
    prev = cur;

  char* name = malloc(strlen(prev) + 1);
  memcpy(name, prev, strlen(prev) + 1);
  return name;
}

//@4-4 F: move_dir
struct dir* move_dir(const char* path_name){
  int length = strlen(path_name);
  char path[length + 1]; //#A learn
  memcpy(path, path_name, length + 1);

  struct dir* dir;
  if(path[0] == '/' || !thread_current()->cur_dir)
    dir = dir_open_root();
  else
    dir = dir_reopen(thread_current()->cur_dir);
  
  char *cur, *ptr, *prev;           //#A if path = A/B, prev = A, cur = B
  prev = strtok_r(path, "/", &ptr); //#A if path = A,   prev = A, cur = \0
  for(cur = strtok_r(NULL, "/", &ptr); cur != NULL;
    prev = cur, cur = strtok_r(NULL, "/", &ptr))
  {
    struct inode* inode;
    if(strcmp(prev, ".") == 0) continue;
    else if(strcmp(prev, "..") == 0)
    {
      inode = dir_parent_inode(dir); //#A return NULL if dir == NULL
      if(inode == NULL) return NULL;
    }
    else if(dir_lookup(dir, prev, &inode) == false)
      return NULL;

    if(inode_is_dir(inode))
    {
      dir_close(dir);
      dir = dir_open(inode);
    }
    else
      inode_close(inode);
  }

  return dir;
}

//@4-4 F: filesys_change_dir()
bool filesys_change_dir(const char *str_dir){
  struct dir *dir = move_dir(str_dir); //#A return NULL, if wrong
  char *file_name = path_to_name(str_dir); //#A file_name need free
  struct inode *inode = NULL;

  if(dir == NULL) {
    free(file_name);
    return false;
  }
  else if(strcmp(file_name, "..") == 0){
    inode = dir_parent_inode(dir);
    if(inode == NULL){
      free(file_name);
      return false;
    }
  }
  else if(strcmp(file_name, ".") == 0 || 
         (strlen(file_name) == 0 && dir_is_root(dir))){
    thread_current()->cur_dir = dir;
    free(file_name);
    return true;
  }
  else 
    dir_lookup(dir, file_name, &inode);
  dir_close(dir);

  dir = dir_open(inode);

  if(dir == NULL) {
    free(file_name);
    return false;
  }
  else{
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = dir;
    free(file_name);
    return true;
  }
}

