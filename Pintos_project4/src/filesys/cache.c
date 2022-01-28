//@4-1 #include
#include "filesys/cache.h"
#include <stdbool.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"
//@4-1 F: cache_very_init
void cache_very_init(){
    lock_init(&arr_lock_cache);
    for (int i = 0; i < CACHE_MAX_SIZE; i++){
        cache[i].valid = false; //#A most-important
        cache[i].sector = -1;
        cache[i].dirty = false;
        cache[i].open_cnt = 0;
    }
    //@4-1 flush-thread
    thread_create("cache_flushing", PRI_DEFAULT, for_cache_flush_thread, NULL);
}
//@4-1 F: get_entry_cache
int get_entry_cache(block_sector_t sector){
    ASSERT(sector != -1);
    lock_acquire(&arr_lock_cache);
    //#A already existed
    for (int i = 0; i < CACHE_MAX_SIZE; i++)
        if (cache[i].sector == sector && cache[i].valid == true){
            cache[i].open_cnt += 1;
            lock_release(&arr_lock_cache);
            return i;
        }
    //#A not existed
    //#A find free
    for (int i = 0; i < CACHE_MAX_SIZE; i++){
        if (cache[i].valid == false){ //#A OK ??

            cache[i].valid = true; 
            cache[i].sector = sector;
            cache[i].dirty = false;
            cache[i].open_cnt = 1;
            block_read(fs_device, sector, cache[i].data);

            lock_release(&arr_lock_cache);
            return i;
        }
    }
    //#A evict, with i random
    for(int i = sector % CACHE_MAX_SIZE; ; i = (i + 1) % CACHE_MAX_SIZE){
        if (cache[i].open_cnt > 0)
            continue;
        
        if (cache[i].valid == true && cache[i].dirty == true){
            ASSERT(cache[i].sector != -1);
            block_write(fs_device, cache[i].sector, cache[i].data);
        }
        cache[i].valid = true; 
        cache[i].sector = sector;
        cache[i].dirty = false;
        cache[i].open_cnt = 1;
        block_read(fs_device, sector, &cache[i].data);

        lock_release(&arr_lock_cache);
        return i;           
    }
    PANIC("cache-get-entry bottom, no reach\n");
}
//@4-1 F: flush_cache
void flush_cache(){
    lock_acquire(&arr_lock_cache);
    for (int i = 0; i < CACHE_MAX_SIZE; i++){
        if(cache[i].valid == true && cache[i].dirty == true){
            block_write(fs_device, cache[i].sector, &cache[i].data);
            cache[i].dirty = false;
        }
        if(cache[i].open_cnt == 0)
            cache[i].valid = false;
    }
    lock_release(&arr_lock_cache);
    return;
}
//@4-4 F: for_cache_flush_thread
void for_cache_flush_thread(void *aux UNUSED){
    while (true){
        timer_sleep(TIMER_FREQ);
        flush_cache();
    }
}
