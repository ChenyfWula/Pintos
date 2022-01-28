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

//@2-2 Global-Val
#define MAX_SYSCALL_NUM 13

unsigned FD = 2;
//@2-4 F: get_fd
unsigned get_fd()
{
  FD++;
  return FD;
}

static void (*sys_func_table[MAX_SYSCALL_NUM])(struct intr_frame *);

static void syscall_handler(struct intr_frame *); //#A Original

//@2-4 F: get_user; from docu, unused
static int get_user (const uint8_t *uaddr){
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}
//@2-2 F:error_exit
static void error_exit()
{
  thread_current()->exit_status = -1;
  thread_exit();
}
//@2-2 F:is_valid_uptr
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
//@2-2 F:is_valid_a1
static bool is_valid_a1(void *esp)
{
  return is_valid_uptr(esp) && is_valid_uptr(esp + 4);
}
//@2-2 F:is_valid_a2
static bool is_valid_a2(void *esp)
{
  return is_valid_uptr(esp) && is_valid_uptr(esp + 4) && is_valid_uptr(esp + 8);
}
//@2-2 F:is_valid_a3
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

//@2-2 F:sc_halt: void halt (void)
static void sc_halt(struct intr_frame *f)
{
  shutdown_power_off();
}
//@2-2 F:sc_exit: void exit (int status)
static void sc_exit(struct intr_frame *f)
{
  if (is_valid_a1(f->esp) == false)
    error_exit();

  int status = *(int *)(f->esp + 4);
  thread_current()->exit_status = status;
  thread_exit();
}

//@2-3 F:sc_exec: pid_t exec (const char *cmd_line)
static void sc_exec(struct intr_frame *f)
{
  //==========validation part===============
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //#A for:exec-bound-3; trouble using strlen, only first 4 byte checked
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  //#X call proper function to do the corresponding job:
  char *cmd_line = *(char **)(f->esp + 4);
  f->eax = process_execute(cmd_line);
}

//@2-3 F:sc_wait: int wait (pid t pid)
static void sc_wait(struct intr_frame *f)
{
  //==========validation part===============
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  tid_t pid = *(tid_t *)(f->esp + 4);
  f->eax = process_wait(pid);
}

//@2-4 F:sc_create: bool create (const char *file, unsigned initial_size)
static void sc_create(struct intr_frame *f)
{
  //==========validation part===============
  if (!is_valid_a2(f->esp))
  {
    f->eax = -1;
    error_exit();
  }

  /*check the context*/
  char *file_name = *(char **)(f->esp + 4);
  if (!is_valid_uptr(file_name))
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============
  unsigned initial_size = *(unsigned *)(f->esp + 8);

  lock_acquire(&file_lock);
  f->eax = filesys_create(file_name, initial_size);
  lock_release(&file_lock);
}

//@2-4 F:sc_remove: bool remove (const char *file)
static void sc_remove(struct intr_frame *f)
{
  //==========validation part===============
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //#A for:exec-bound-3; trouble using strlen
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============
  char *file_name = *(char **)(f->esp + 4);

  lock_acquire(&file_lock);
  f->eax = filesys_remove(file_name);
  lock_release(&file_lock);
}

//@2-4 F:sc_open: int open (const char *file)
static void sc_open(struct intr_frame *f)
{
  //==========validation part===============
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //#A for:exec-bound-3; trouble using strlen
  if (is_valid_uptr(*(void **)(f->esp + 4)) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============
  char *file_name = *(char **)(f->esp + 4);

  lock_acquire(&file_lock);
  struct file *Open_f = filesys_open(file_name);
  lock_release(&file_lock);
  if (!Open_f)
  {
    f->eax = -1;
    //#A error_exit();
  }
  else
  {
    struct fd_struct *fds = (struct fd_struct *)malloc(sizeof(struct fd_struct));
    // if open fail
    if (!fds)
    {
      lock_acquire(&file_lock);
      file_close(Open_f);
      lock_release(&file_lock);
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

//@2-4 F:sc_filesize: int filesize (int fd)
static void sc_filesize(struct intr_frame *f)
{
  //==========validation part===============
  if (is_valid_a1(f->esp) == false)
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  else
  {
    int fd = *(int *)(f->esp + 4);

    struct fd_struct *fds = find_fd_struct(fd);
    if (fds)
    {
      f->eax = file_length(fds->file);
    }
    else
    {
      f->eax = -1;
    }
  }
}

//@2-4 F:sc_read: int read (int fd, void *buffer, unsigned size)  
static void sc_read(struct intr_frame *f)
{
  //==========validation part=============== 
  if (!is_valid_a3(f->esp)){
    f->eax = -1;
    error_exit();
  }

  int fd = *(int *)(f->esp + 4);
  void *buffer = *(void **)(f->esp + 8);
  uint32_t size = *(uint32_t *)(f->esp + 12);

  if (is_valid_uptr(buffer) == false || is_valid_uptr(buffer + size) == false){
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  //call proper function to do the corresponding job:
  if(fd == 0){
    lock_acquire(&file_lock);
    for (int i = 0; i < size; i++)
      *(char *)(buffer+i) = (char)input_getc(); //#A ??
    lock_release(&file_lock);
    f->eax = size;
    return;
  }
  struct fd_struct *fds = find_fd_struct(fd);
  if(!fds){
    f->eax = -1;
    return;
    //error_exit();
  }
  lock_acquire(&file_lock);
  f->eax = file_read(fds->file, buffer, size);
  lock_release(&file_lock);
}

//@2-4 F:sc_write: int write (int fd, const void *buffer, unsigned size)
static void sc_write(struct intr_frame *f)
{
  //==========validation part===============
  if (!is_valid_a3(f->esp))
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation for write to console finish=============
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
      //==========validation part=============
      if (is_valid_uptr(buffer) == false || is_valid_uptr(buffer + size) == false){
        f->eax = -1;
        error_exit();
      }
      //==========validation for normal write finish=============

      //call proper function to do the corresponding job:
      struct fd_struct *fds = find_fd_struct(fd);
      if(!fds){
        f->eax = -1;
        return;
        //error_exit();
      }
      lock_acquire(&file_lock);
      f->eax = file_write(fds->file, buffer, size);
      lock_release(&file_lock);
    }
  }
}

//@2-4 F:sc_seek: void seek (int fd, unsigned position)
static void sc_seek(struct intr_frame *f)
{
  //==========validation part===============
  if (!is_valid_a2(f->esp))
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  //call proper function to do the corresponding job:
  int fd = *(int *)(f->esp + 4);
  unsigned position = *(unsigned *)(f->esp + 8);
  struct fd_struct *fds = find_fd_struct(fd);
  if (fds)
  {
    lock_acquire(&file_lock);
    file_seek(fds->file, position);
    lock_release(&file_lock);
  }
  else
  {
    error_exit();
  }
}
//@2-4 F:sc_tell: unsigned tell (int fd)
static void sc_tell(struct intr_frame *f)
{
  //==========validation part===============
  if (!is_valid_a1(f->esp))
  {
    f->eax = -1;
    error_exit();
  }
  //==========validation finish=============

  //call proper function to do the corresponding job:

  int fd = *(int *)(f->esp + 4);

  struct fd_struct *fds = find_fd_struct(fd);
  if (fds)
  {
    lock_acquire(&file_lock);
    f->eax = file_tell(fds->file);
    lock_release(&file_lock);
  }
  else
  {
    f->eax = -1;
  }
}

//@2-4 F: is_file_opening: check whether the file is held by a certain thread.
//#A Used in sc_close().
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

//@2-4 F:sc_close:void close (int fd)
static void sc_close(struct intr_frame *f)
{
  //==========validation part===============
  if (!is_valid_a1(f->esp))
    error_exit();
  //==========validation finish=============

  //call proper function to do the corresponding job:
  int fd = *(int *)(f->esp + 4);

  struct fd_struct *fds = find_fd_struct(fd);
  if (!fds)
    error_exit();

  lock_acquire(&file_list_lock);
  list_remove(&fds->file_elem);
  lock_release(&file_list_lock);
  
  list_remove(&fds->thread_fd_elem);

  if(is_file_opening(fds->file)==false){
    lock_acquire(&file_lock);
    file_close(fds->file);
    lock_release(&file_lock);
  }
  free(fds);
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

  //@2-4 file lock init
  lock_init(&file_lock);
  lock_init(&file_list_lock);
  list_init(&file_list);
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
