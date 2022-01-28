#ifndef SWAP_H
#define SWAP_H

#include "devices/block.h"
#include "frame.h"
#include "sup_table.h"
//@3-3 Global-Val
struct block *swap_slot;
struct bitmap *swap_bitmap;
struct lock swap_lock;

//@3-3 F: swap_init
void swap_init(void); 

/*write a page to the swap, return the corresponding block_sector_t, which is a number.
so that in the supplemental table we can have  kpage<---->sector.
To be more sepcified, sector is the location in the swap slot that kpage is swapped.
So we can use sector/PAGE_SECTOR_NUM to get the corresponding bitmap bit*/
block_sector_t page_write_swap (void *kpage);

//read a page onto the kpage from the block_sector_t
void page_read_swap (block_sector_t sector, void *kpage);

//remove the certain sector in the swap slot.
void swap_remove (block_sector_t sector);

//@3-3 F: frame_swap
uint8_t *frame_swap(struct frame *evict_f);
#endif
