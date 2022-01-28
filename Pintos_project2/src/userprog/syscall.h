#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
//@2-2 #include
#include <stdbool.h>
#include <stdint.h>
#include "threads/synch.h"
//@2-4 #include
#include "filesys/file.h"

//@2-4 Global-Val
struct lock file_lock;
struct list file_list;
struct lock file_list_lock;

//@2-2 F: is_vaild_uptr
bool is_vaild_uptr(void *ptr);
void syscall_init (void);
//@2-4 F: is_file_opening
bool is_file_opening(struct file *file_2close);

#endif /* userprog/syscall.h */
