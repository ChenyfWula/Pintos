#ifndef SUP_TABLE_H
#define SUP_TABLE_H
//@3-1 #include
#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include "vm/frame.h"
#include "threads/interrupt.h"
#include "vm/swap.h"
#define STACK_LOW_BOUND 0xbf800000
//@3-4 #include
#include "vm/mmap.h"

enum page_status{
    NORMAL, //#A 0
    SWAP  //#A 1
};

//@3-1 S: page_info
struct page_info{
    uint8_t* uvaddr;
    enum page_status status; //#A start as 0?
    bool writable; //#A dirty & access in pagedir.h
    struct thread *allocator;

    struct list_elem elem_in_thread;
    struct list_elem elem_in_frame; //#A we may impl share
    struct frame *phy_frame;   
    //@3-3 in: page_info
    block_sector_t slot_sector;
    //@3-4 in: page_info
    bool mapped;
    struct file_map *map;
};
//@3-1 F: page_info_create
struct page_info *page_info_create(uint8_t* uvaddr, struct frame *phy_frame, bool writable);
//@3-1 F: page_info_free
bool page_info_free(struct page_info *to_free);

//@3-1 F: find_page_info
struct page_info *find_page_info(uint8_t* uvaddr, struct thread *t UNUSED); 

//@3-2 F: stack_grow_loop
bool ustack_grow_loop(void *fault_addr, void *ustack);
//@3-2 F: stack_find
uint8_t *stack_page_find();

#endif //#A vm/sup_table.h