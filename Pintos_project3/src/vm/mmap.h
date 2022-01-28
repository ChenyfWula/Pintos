#ifndef MMAP_H
#define MMAP_H
//@3-4 #include
#include "userprog/syscall.h"
#include "filesys/file.h"
#include <list.h>
#include <stdbool.h>
#include "vm/sup_table.h"
#include <stdint.h>
//@3-4 #include, red-wave
#include <inttypes.h>

//@3-4 Global-Val
typedef int mapid_t;    /*type define of map id*/
mapid_t mid_global;     /*used to assgine value for the each map*/

//@3-4 S: file_map      
struct file_map{
    mapid_t mid;                        /*map id*/
    int len;                            /*the lenth of the file, in bytes. Warning: not the number of pages!*/
    void *addr_start;                   /*the start address of the page*/
    struct file *file_mapped;           /*the file we have mapped*/
    struct list_elem elem_in_thread;    /*In thread.c we have: mmap_list. To record what we have mapped*/
};

/*initialize function, every simple one*/
void mmap_very_init();     


//@3-4 F: file_map_create
/*create memory map given the fd and the address*/
mapid_t file_map_create(int fd, void *addr);    

//@3-4 F: file_page_read
/*read one page from file, and write it to the kpage*/
bool file_page_read(struct page_info *page_i, uint8_t *kpage); 
//@3-4 F: file_page_write
/*read one page from kpage, and write it to the file*/
bool file_page_write(struct page_info *page_i, uint8_t *kpage);

//@3-4 F: file_map_free
/*free the map given mid*/
bool file_map_free(mapid_t mid);    

//@3-4 F: find_file_map
/*find the corresponding map given the mid*/
struct file_map *find_file_map(mapid_t mid);    
//@3-4 F: free_mmap
/*free all of the map we have created in the current thread*/
void free_mmap();                               
#endif /* vm/mmap.h */