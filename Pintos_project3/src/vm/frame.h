#ifndef FRAME_H
#define FRAME_H
//@3-1 #include
#include <debug.h>
#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <hash.h>
#include "threads/synch.h"

//@3-1 Global-Val
struct hash frame_hash; 
struct lock lock_frame_hash;

//@3-1 S: frame
struct frame{
    uint8_t* kvaddr;
    struct hash_elem elem_in_hash;
    struct list upage_list; //#A only one element, if no-share
    uint32_t create_time;
};

//@3-1 F: hash
unsigned frame_kvaddr_hash(const struct hash_elem *e, void *aux UNUSED);
//@3-1 F: less_hash
bool frame_kvaddr_less_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

//@3-1 F: frame_very_init
bool frame_very_init();
//@3-1 F: frame_create
struct frame *frame_create(uint8_t* kvaddr);
//@3-1 F: frame_free, when palloc-free
bool frame_free(uint8_t* kvaddr);

//@3-1 F: find_frame
struct frame *find_frame(uint8_t* kvaddr);

//@3-3 F: find_evict
struct frame *find_evict();
//@3-3 F: show_upage, debug
void show_upage();

#endif //#A vm/frame.h