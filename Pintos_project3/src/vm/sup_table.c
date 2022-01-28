//@3-1 #include
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/sup_table.h"
//@3-2 #include
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

//@3-1 F: page_info_create
struct page_info *page_info_create(uint8_t* uvaddr, 
                                struct frame *phy_frame, bool writable){
    ASSERT(is_user_vaddr(uvaddr) && !pg_ofs(uvaddr));
    //#A ASSERT(phy_frame); Lazy load

    struct thread *cur = thread_current();
    //@3-3 in: page_info_create
    struct page_info *existed = find_page_info(uvaddr, cur);
    if(existed != NULL){
        existed->status = NORMAL;
        ASSERT(phy_frame);
        list_push_front(&phy_frame->upage_list, &existed->elem_in_frame);
        existed->phy_frame = phy_frame;
        existed->slot_sector = 0;
        return existed;
    }

    struct page_info *new_page_info = malloc(sizeof(struct page_info));
    if(new_page_info == NULL)
        return NULL;
    new_page_info->uvaddr = uvaddr;
    //@3-4  in: page_info_create
    if(phy_frame != NULL){
        new_page_info->status = NORMAL;
        list_push_front(&phy_frame->upage_list, &new_page_info->elem_in_frame);
        new_page_info->mapped = false;
        new_page_info->map = NULL;
    }   
    else{
        new_page_info->status = SWAP;
        new_page_info->mapped = true;
    }
    new_page_info->writable = writable;
    new_page_info->allocator = cur;
    list_push_front(&cur->page_list, &new_page_info->elem_in_thread);
    new_page_info->phy_frame = phy_frame;
    //@3-3 in: page_info_create
    new_page_info->slot_sector = 0;

    return new_page_info;
}
//@3-1 F: page_info_free
bool page_info_free(struct page_info *to_free){
    ASSERT(to_free);
    list_remove(&to_free->elem_in_thread);
    if(to_free->status == NORMAL)
        list_remove(&to_free->elem_in_frame);
    free(to_free);
    return true;
}

//@3-1 F: find_page_info, round-inside
struct page_info *find_page_info(uint8_t* uvaddr, struct thread *t UNUSED){
    if(!is_user_vaddr(uvaddr))
        return NULL;

    struct thread *cur = thread_current();
    struct list_elem *e;
    struct page_info *page_i;
    uint8_t* page_uvaddr = pg_round_down(uvaddr);
    for(e = list_begin(&cur->page_list);
        e != list_end(&cur->page_list); e = list_next(e)){
        page_i = list_entry(e, struct page_info, elem_in_thread);
        uint8_t *i_uvaddr = page_i->uvaddr; //#A for debug
        if(i_uvaddr == page_uvaddr)
            return page_i;
    }
    return NULL;
}

//@3-2 F: stack_grow_loop
bool ustack_grow_loop(void *fault_addr, void *ustack){
    struct thread *cur = thread_current();
    uint8_t *start_upage = stack_page_find();
    uint8_t *dest_upage = pg_round_down(fault_addr);
    uint8_t *kpage;

    while(start_upage > dest_upage){
        start_upage -= PGSIZE;
        kpage = palloc_get_page (PAL_USER | PAL_ZERO);
        //@3-3 in: ustack_grow_loop
        if(kpage == NULL)
            kpage = frame_swap(find_evict());
        bool success = pagedir_set_page(cur->pagedir, start_upage, kpage, true);
        if (success)
            page_info_create(start_upage, frame_create(kpage), true);
        else{
            palloc_free_page (kpage);
            return false;
        }
    }
    return true;
}
//@3-2 F: stack_find
uint8_t *stack_page_find(){
    struct thread *cur = thread_current();
    struct list_elem *e;
    struct page_info *page_i;
    uint8_t *stack_page = 0xbffff000;
    for(e = list_begin(&cur->page_list);
        e != list_end(&cur->page_list); e = list_next(e)){
        page_i = list_entry(e, struct page_info, elem_in_thread);
        uint8_t *i_uvaddr = page_i->uvaddr; //#A for debug
        if(i_uvaddr  >= STACK_LOW_BOUND && i_uvaddr < stack_page)
            return page_i;
    }
    return stack_page;
}