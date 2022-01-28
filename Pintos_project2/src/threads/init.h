#ifndef THREADS_INIT_H
#define THREADS_INIT_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*Memory leak testing variables' decleration*/
// int as_child_malloc_t;
// int as_child_free_t;
// int fn_copy_malloc_t;
// int fn_copy_free_t;
// int name_copy_malloc_t;
// int name_copy_free_t;
// int fds_malloc_t;
// int fds_free_t;
/* Page directory with kernel mappings only. */
extern uint32_t *init_page_dir;

#endif /* threads/init.h */
