//@3-3 #include
#include <stdbool.h>
#include <debug.h>
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#define PAGE_SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)
//@3-4 #include
#include "vm/mmap.h"
//@3-4 debug
#include "userprog/syscall.h"
#include <debug.h>

static bool swap_allocation(block_sector_t *sector, uint32_t page_num);

// swap slot initialization
void swap_init(void)
{
    swap_slot = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(swap_slot) / PAGE_SECTOR_NUM);
    lock_init(&swap_lock);
}

block_sector_t page_write_swap(void *kpage)
{
    int cnt;
    block_sector_t sector;

    // get one page ready for writing
    // using a lock to ensure that only one process is adjusting the swap slot
    lock_acquire(&swap_lock);
    if (!swap_allocation(&sector, 1))
        PANIC("Not enough space in swap slot");
    /*one page contains PAGE_SECTOR_NUM sectors, so we need to write it to swap_slot one by one*/
    for (cnt = 0; cnt < PAGE_SECTOR_NUM; cnt++)
    {
        block_write(swap_slot, sector, kpage);
        sector++;
        kpage += BLOCK_SECTOR_SIZE;
    }
    lock_release(&swap_lock);
    /*return the head*/
    return sector - PAGE_SECTOR_NUM;
}

/*loads the data in the swap slot to the kapage, as we have mapped sector<----->kpage*/
void page_read_swap(block_sector_t sector, void *kpage)
{
    int cnt;

    if (!bitmap_test(swap_bitmap, sector / PAGE_SECTOR_NUM))
    {
        PANIC("Swap slot doesn't contain the sector");
    }
    lock_acquire(&swap_lock);
    swap_remove(sector);
    for (cnt = 0; cnt < PAGE_SECTOR_NUM; cnt++)
    {
        block_read(swap_slot, sector, kpage);
        sector++;
        kpage += BLOCK_SECTOR_SIZE;
    }
    lock_release(&swap_lock);
}

/*remove the certain sector in the swap slot.*/
void swap_remove(block_sector_t sector)
{
    /*test whether the sector is in the swap slot, if true remove it*/
    if (bitmap_test(swap_bitmap, sector / PAGE_SECTOR_NUM))
        bitmap_reset(swap_bitmap, sector / PAGE_SECTOR_NUM);
    /*we don't need to remove things in the swap_slot because
    if the bitmap is zero, the data won't be got from the swap_slot*/
}

static bool swap_allocation(block_sector_t *sector, uint32_t page_num)
{
    block_sector_t first_bit = bitmap_scan_and_flip(swap_bitmap, 0, page_num, false);
    if (first_bit != BITMAP_ERROR)
        *sector = first_bit * PAGE_SECTOR_NUM;
    return (first_bit != BITMAP_ERROR);
}
//@3-3 F: frame_swap
uint8_t *frame_swap(struct frame *evict_f){
    ASSERT(evict_f);
    
    struct list_elem *e = list_pop_front(&evict_f->upage_list);
    struct page_info *swp_page = list_entry(e, struct page_info, elem_in_frame);
    struct thread *orig_alo = swp_page->allocator;
    uint8_t *swap_kpage = evict_f->kvaddr;
    uint8_t *swap_upage = swp_page->uvaddr;

    swp_page->status = SWAP;
    swp_page->phy_frame = NULL;
    //@3-4 in: frame_swap
    if(swp_page->mapped == true && 
            pagedir_is_dirty(orig_alo->pagedir, swap_upage)){
        pagedir_clear_page(orig_alo->pagedir, swap_upage); 
        file_page_write(swp_page, swap_kpage);
    }
    else if(swp_page->mapped != true){
        pagedir_clear_page(orig_alo->pagedir, swap_upage); 
        swp_page->slot_sector = page_write_swap(swap_kpage);
    }   
    else
        pagedir_clear_page(orig_alo->pagedir, swap_upage);
    //ASSERT(pg_ofs(swap_upage) == 0);
    //pagedir_clear_page(orig_alo->pagedir, swap_upage);
    //#A if orig want to swap back, it need the same lock.
    
    free(evict_f);
    return swap_kpage;
}