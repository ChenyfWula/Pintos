#ifndef CACHE_H
#define CACHE_H
//@4-1 #include
#include <stdbool.h>
#include "devices/block.h"
#include "threads/synch.h"
#define CACHE_MAX_SIZE 64
//@4-1 S: cache_entry
struct cache_entry{
    block_sector_t sector;
    bool valid;
    uint32_t open_cnt;    //#C whether the cache is in use
    bool dirty; //#A write-back now, no-use
    uint8_t data[BLOCK_SECTOR_SIZE];
};
//@4-1 Global-Val
struct cache_entry cache[CACHE_MAX_SIZE]; //#A OK ??
struct lock arr_lock_cache; //#A Don't, when holding any cache_lock

//@4-1 F: cache_very_init
void cache_very_init(void);
//@4-1 F: get_entry_cache
int get_entry_cache(block_sector_t sector);
//@4-1 F: flush_cache
void flush_cache();
//@4-4 F: for_cache_flush_thread
void for_cache_flush_thread(void *);

#endif 
