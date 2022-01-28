#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
//@2-2 #include
#include <stdbool.h>
#include <stdint.h>
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include <console.h>
//@2-3 #include
#include "userprog/process.h"
#include <string.h>
#include <list.h>
//@2-4 #include
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/init.h"
//@4-4 #include
#include "filesys/directory.h"

//@2-2 Global-Val
//@4-4 Global-Val
#define MAX_SYSCALL_NUM 20

//@4-0 update FD
static uint32_t global_fid = 2;
struct lock fid_lock;
//@2-4 F: get_fd
static uint32_t get_fd(void){
  uint32_t cur_fid;
  lock_acquire(&fid_lock);
  cur_fid = global_fid++;
  lock_release(&fid_lock);
  return cur_fid;
}

static void (*sys_func_table[MAX_SYSCALL_NUM])(struct intr_frame *);

static void syscall_handler(struct intr_frame *); //#A Original

//@2-4 F: get_user; from docu, unused
static int get_user (const uint8_t *uaddr){
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}
//@2-2 F: error_exit
static void error_exit()
{
  thread_current()->exit_status = -1;
  thread_exit();
}
//@2-2 F: is_valid_uptr
bool is_valid_uptr(void *uptr)
{
  if (uptr == NULL)
    return false;
  if (is_user_vaddr(uptr) == false)
    return false;
  //#A for: sc-boundary-3
  for (int i = 0; i < 4; i++)
  {
    if (pagedir_get_page(thread_current()->pagedir, uptr + i) == NULL)
      return false;
  }
  return true;
}
//@2-2 F: is_valid_a1
static bool is_valid_a1(void *esp)
{
  return is_valid_uptr(esp) && is_valid_uptr(esp + 4);
}
//@2-2 F: is_valid_a2
static bool is_valid_a2(void *esp)
{
  return is_valid_uptr(esp) && is_valid_uptr(esp + 4) && is_valid_uptr(esp + 8);
}
//@2-2 F: is_valid_a3
static bool is_valid_a3(void *esp)
{
  return is_valid_uptr(esp) && is_valid_uptr(esp + 4) && is_valid_uptr(esp + 8) && is_valid_uptr(esp + 12);
}


// @2-4 F: find_fd_struct_inThread
static struct fd_struct *find_fd_struct(int fd)
{
  struct thread *cur = thread_current();
  struct list_elem *e;
  struct fd_struct *fds;
  for (e = list_begin(&cur->fd_list); e != list_end(&cur->fd_list); e = list_next(e))
  {
    fds = list_entry(e, struct fd_struct, thread_fd_elem);
    if (fds->fd == fd)
    {
      return fds;
    }
  }
  return NULL;
}

//@2-2 F: sc_halt: void halt (void)
static void sc_halt(struct intr_frame *f)
{
  shutdown_power_off();
}
//@2-2 F: sc_exit: void exit (int status)
static void sc_exit(struct intr_frame *f)
{
  if (is_valid_a1(f->esp) == false)
    error_exit();
  //#A ====== Valid-finished ======
  int status = *(int *)(f->esp + 4);
  thread_current()->exit_status = status;
  thread_exit();
}

//@2-3 F: sc_exec: pid_t exec (const char *cmd_line)
static void sc_exec(struct intr_frame *f)
{
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  //#A for:exec-bound-3; trouble using strlen, only first 4 byte checked
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
    error_exit();

  //#A ====== Valid-finished ======
  char *cmd_line = *(char **)(f->esp + 4);
  f->eax = process_execute(cmd_line);
}

//@2-3 F: sc_wait: int wait (pid_t pid)
static void sc_wait(struct intr_frame *f)
{
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  //#A ====== Valid-finished ======
  tid_t pid = *(tid_t *)(f->esp + 4);
  f->eax = process_wait(pid);
}

//@2-4 F: sc_create: bool create (const char *file, unsigned initial_size)
static void sc_create(struct intr_frame *f)
{
  f->eax = (uint32_t) false; //@4-0
  if (!is_valid_a2(f->esp))
    error_exit();
  /*check the context*/
  char *file_name = *(char **)(f->esp + 4);
  if (!is_valid_uptr(file_name))
    error_exit();
  //#A ====== Valid-finished ======
  unsigned initial_size = *(unsigned *)(f->esp + 8);
  //@4-4 C: FC.d.ver
  f->eax = filesys_create(file_name, initial_size, false);
  return;
  
  //@4-y remove-lock
  //lock_acquire(&file_lock);
  //lock_release(&file_lock);
}

//@2-4 F: sc_remove: bool remove (const char *file)
static void sc_remove(struct intr_frame *f)
{
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = (uint32_t)false; //@4-0
    error_exit();
  }
  //#A for:exec-bound-3; trouble using strlen
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
  {
    f->eax = (uint32_t)false; //@4-0
    error_exit();
  }
  //#A ====== Valid-finished ======
  char *file_name = *(char **)(f->esp + 4);
  //@4-y remove-lock
  //lock_acquire(&file_lock);
  f->eax = filesys_remove(file_name);
  //lock_release(&file_lock);
}

//@2-4 F: sc_open: int open (const char *file)
static void sc_open(struct intr_frame *f)
{
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  //#A for:exec-bound-3; trouble using strlen
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
    error_exit();
  //#A ====== Valid-finished ======
  char *file_name = *(char **)(f->esp + 4);
  //@4-y remove-lock
  //lock_acquire(&file_lock);
  struct file *Open_f = filesys_open(file_name);
  //lock_release(&file_lock);
  if (Open_f)
  {
    struct fd_struct *fds = (struct fd_struct *)malloc(sizeof(struct fd_struct));
    // if open fail
    if (!fds)
    {
      //@4-y remove-lock
      //lock_acquire(&file_lock);
      file_close(Open_f);
      //lock_release(&file_lock);
      f->eax = -1;
    }
    else
    {
      fds->fd = get_fd();
      fds->file = Open_f;
      
      list_push_back(&thread_current()->fd_list, &fds->thread_fd_elem);
      lock_acquire(&file_list_lock);
      list_push_back(&file_list, &fds->file_elem);
      lock_release(&file_list_lock);
      f->eax = fds->fd;
    }
  }
}

//@2-4 F: sc_filesize: int filesize (int fd)
static void sc_filesize(struct intr_frame *f)
{
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  //#A ====== Valid-finished ======
  else
  {
    int fd = *(int *)(f->esp + 4);

    struct fd_struct *fds = find_fd_struct(fd);
    if (fds)
    {
      f->eax = file_length(fds->file);
    }
  }
}

//@2-4 F: sc_read: int read (int fd, void *buffer, unsigned size)  
static void sc_read(struct intr_frame *f)
{
  f->eax = -1;
  if (!is_valid_a3(f->esp))
    error_exit();
  

  int fd = *(int *)(f->esp + 4);
  void *buffer = *(void **)(f->esp + 8);
  uint32_t size = *(uint32_t *)(f->esp + 12);

  if (is_valid_uptr(buffer) == false || is_valid_uptr(buffer + size) == false)
    error_exit();
  
  //#A ====== Valid-finished ======
  if(fd == 0){
    //@4-y remove-lock
    //lock_acquire(&file_lock);
    for (int i = 0; i < size; i++)
      *(char *)(buffer+i) = (char)input_getc(); //#A ??
    //lock_release(&file_lock);
    f->eax = size;
    return;
  }
  struct fd_struct *fds = find_fd_struct(fd);
  if(!fds){
    return;
    //error_exit();
  }
  //@4-4 cannot read to directory
  if (inode_is_dir(file_get_inode(fds->file)))
    return;

  //@4-y remove-lock
  //lock_acquire(&file_lock);
  f->eax = file_read(fds->file, buffer, size);
  //lock_release(&file_lock);
}

//@2-4 F: sc_write: int write (int fd, const void *buffer, unsigned size)
static void sc_write(struct intr_frame *f)
{
  f->eax = -1;
  if (!is_valid_a3(f->esp))
    error_exit();

  //#A validation for write to console finish
  else
  {
    int fd = *(int *)(f->esp + 4);
    void *buffer = *(void **)(f->esp + 8);
    uint32_t size = *(uint32_t *)(f->esp + 12);

    if (fd == 1)
    {
      putbuf(buffer, size);
      f->eax = size;
    }
    else
    {
      if (is_valid_uptr(buffer) == false || is_valid_uptr(buffer + size) == false)
        error_exit();
      //#A ====== Valid-finished ======
      struct fd_struct *fds = find_fd_struct(fd);
      if(!fds)
        return;
        //error_exit();
      //@4-y remove-lock
      //@4-4 cannot read to directory
      if (inode_is_dir(file_get_inode(fds->file)))
        return;
      //lock_acquire(&file_lock);
      f->eax = file_write(fds->file, buffer, size);
      //lock_release(&file_lock);
    }
  }
}

//@2-4 F: sc_seek: void seek (int fd, unsigned position)
static void sc_seek(struct intr_frame *f)
{
  f->eax = -1;
  if (!is_valid_a2(f->esp))
    error_exit();
  //#A ====== Valid-finished ======
  int fd = *(int *)(f->esp + 4);
  unsigned position = *(unsigned *)(f->esp + 8);
  struct fd_struct *fds = find_fd_struct(fd);
  if (fds)
  {
    //@4-y remove-lock
    //lock_acquire(&file_lock);
    //@4-4 cannot seek on directory
    if (inode_is_dir(file_get_inode(fds->file)))
        return;
    file_seek(fds->file, position);
    //lock_release(&file_lock);
  }
  else
  {
    error_exit();
  }
}
//@2-4 F: sc_tell: unsigned tell (int fd)
static void sc_tell(struct intr_frame *f)
{
  f->eax = -1;
  if (!is_valid_a1(f->esp))
    error_exit();
  //#A ====== Valid-finished ======
  int fd = *(int *)(f->esp + 4);

  struct fd_struct *fds = find_fd_struct(fd);
  if (fds)
  {
    //@4-y remove-lock
    //lock_acquire(&file_lock);
    //@4-4 cannot tell on directory
    if (inode_is_dir(file_get_inode(fds->file)))
        return;
    f->eax = file_tell(fds->file);
    //lock_release(&file_lock);
  }

}

//@2-4 F: is_file_opening, no-use
bool is_file_opening(struct file *file_2close){
  struct list_elem *e;
  struct fd_struct *fd;
  lock_acquire(&file_list_lock);
  for (e = list_begin(&file_list); e != list_end(&file_list); e = list_next(e)){
    fd = list_entry(e, struct fd_struct, file_elem);
    if(fd->file == file_2close){
      lock_release(&file_list_lock);
      return true;
    }
  }
  lock_release(&file_list_lock);
  return false;
}

//@2-4 F: sc_close:void close (int fd)
static void sc_close(struct intr_frame *f)
{
  if (!is_valid_a1(f->esp))
    error_exit();
  //#A ====== Valid-finished ======
  int fd = *(int *)(f->esp + 4);

  struct fd_struct *fds = find_fd_struct(fd);
  if (!fds)
    error_exit();

  lock_acquire(&file_list_lock);
  list_remove(&fds->file_elem);
  lock_release(&file_list_lock);
  
  list_remove(&fds->thread_fd_elem);
  //@4-0 no-check opening
  //@4-y remove-lock
  //lock_acquire(&file_lock);
    //@4-4 cnnot seek on directory
  if (inode_is_dir(file_get_inode(fds->file)))
    dir_close(fds->file);
  else
    file_close(fds->file);
  //lock_release(&file_lock);
  
  // if(is_file_opening(fds->file)==false){
  //   lock_acquire(&file_lock);
  //   file_close(fds->file);
  //   lock_release(&file_lock);
  // }
  free(fds);
}

//@4-4 F: sc_chdir: bool chdir (const char *dir)
static void sc_chdir(struct intr_frame *f){
  f->eax = -1;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
    error_exit();
  //#A ====== Valid-finished ======
  char* dir = *(char **)(f->esp + 4);
  f->eax = filesys_change_dir(dir);
  return;
}

//@4-4 F: sc_mkdir: bool mkdir (const char *dir)
static void sc_mkdir(struct intr_frame *f){
  f->eax = (uint32_t) false;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
    error_exit();
  //#A ====== Valid-finished ======
  char* dir = *(char **)(f->esp + 4);
  f->eax = filesys_create(dir, 16 * sizeof_dir_entry(), true);
  return;
}
//@4-4 F: sc_readdir: bool readdir (int fd, char name[READDIR_MAX_LEN + 1])
static void sc_readdir(struct intr_frame *f){
  f->eax = (uint32_t) false;
  if (!is_valid_a2(f->esp)) 
    error_exit();
  int fd = *(int *)(f->esp + 4);
  void *name = *(void **)(f->esp + 8);

  if (is_valid_uptr(name) == false || 
      is_valid_uptr(name + NAME_MAX + 1) == false)
    error_exit();
  struct fd_struct *fds = find_fd_struct(fd);
  if(fds == NULL)
    return;
  //#A ====== Valid-finished ======
  struct inode* inode = file_get_inode(fds->file);
  if(inode == NULL)
    return;
  if(!inode_is_dir(inode))
    return;
  struct dir* dir = (struct dir*) fds->file;
  f->eax = dir_readdir(dir, name);
  //dir_close(file_get_inode(fds->file));
  return;
}
//@4-4 F: sc_isdir: bool isdir (int fd)
static void sc_isdir(struct intr_frame *f){
  f->eax = (uint32_t) false;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  int fd = *(int *)(f->esp + 4);
  struct fd_struct *fds = find_fd_struct(fd);
  if(fds == NULL)
    return;
  //#A ====== Valid-finished ======
  struct inode* inode = file_get_inode(fds->file);
  if(inode == NULL)
    return;
  f->eax = inode_is_dir(inode);
  return;
}
//@4-4 F: sc_inumber: bool inumber (int fd) 
static void sc_inumber(struct intr_frame *f){
  f->eax = (uint32_t) false;
  if (is_valid_a1(f->esp) == false)
    error_exit();
  int fd = *(int *)(f->esp + 4);
  struct fd_struct *fds = find_fd_struct(fd);
  if(fds == NULL)
    return;
  //#A ====== Valid-finished ======
  struct inode* inode = file_get_inode(fds->file);
  if(inode == NULL)
    return;
  f->eax = inode_get_inumber(inode);
  return;
}

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

  //@2-2 in:syscall_init
  //#A init sys_func_table
  sys_func_table[0] = sc_halt;
  sys_func_table[1] = sc_exit;
  sys_func_table[2] = sc_exec;
  sys_func_table[3] = sc_wait;
  sys_func_table[4] = sc_create;
  sys_func_table[5] = sc_remove;
  sys_func_table[6] = sc_open;
  sys_func_table[7] = sc_filesize;
  sys_func_table[8] = sc_read;
  sys_func_table[9] = sc_write;
  sys_func_table[10] = sc_seek;
  sys_func_table[11] = sc_tell;
  sys_func_table[12] = sc_close;
  //@4-4 in:syscall_init
  sys_func_table[15] = sc_chdir;
  sys_func_table[16] = sc_mkdir;
  sys_func_table[17] = sc_readdir;
  sys_func_table[18] = sc_isdir;
  sys_func_table[19] = sc_inumber;

  //@2-4 file lock init
  lock_init(&file_lock);
  lock_init(&file_list_lock);
  list_init(&file_list);
  //@4-0 update FD
  lock_init(&fid_lock);
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //@2-2 in:syscall_handler
  uint32_t *stack_ptr = f->esp; //#A different in +-1
  if (is_valid_uptr(stack_ptr) == false)
    error_exit(); //#A exit_status = -1 & thread_exit()
  int syscall_no = *(int *)stack_ptr;
  if (syscall_no < 0 || syscall_no >= MAX_SYSCALL_NUM)
    error_exit(); 
  sys_func_table[syscall_no](f);
  // printf ("system call!\n"); //#A Original
  // thread_exit (); //#A Original
}
