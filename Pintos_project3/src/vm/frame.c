//@3-1 #include
#include "vm/frame.h"
#include "threads/vaddr.h"
//@3-3 #include
#include "threads/thread.h"
#include "vm/sup_table.h"
#include "userprog/pagedir.h"

static uint32_t create_time_global;
//@3-1 F: hash
unsigned frame_kvaddr_hash(const struct hash_elem *e, void *aux UNUSED){
    struct frame *frame_to_hash = hash_entry(e, struct frame, elem_in_hash);
    return (uint32_t)frame_to_hash->kvaddr;
}
//@3-1 F: less_hash
bool frame_kvaddr_less_hash(const struct hash_elem *a, 
                            const struct hash_elem *b, void *aux UNUSED){
    struct frame *frame_a = hash_entry(a, struct frame, elem_in_hash);
    struct frame *frame_b = hash_entry(b, struct frame, elem_in_hash);
    return (uint32_t)frame_a->kvaddr < (uint32_t)frame_b->kvaddr;
}

//@3-1 F: frame_very_init
bool frame_very_init(){
    hash_init(&frame_hash, frame_kvaddr_hash, frame_kvaddr_less_hash, NULL);
    lock_init(&lock_frame_hash);
    create_time_global = 0;
}
//@3-1 F: frame_create
struct frame *frame_create(uint8_t* kvaddr){
    ASSERT(is_kernel_vaddr(kvaddr) && !pg_ofs(kvaddr));

    struct frame *new_frame = malloc(sizeof(struct frame));
    if(new_frame==NULL)
        return NULL;
    new_frame->kvaddr = kvaddr;
    list_init(&new_frame->upage_list);
    
    //@3-3 in: frame_create
    struct hash_elem *elem_from_insert;
    lock_acquire(&lock_frame_hash);

    elem_from_insert = hash_insert(&frame_hash, &new_frame->elem_in_hash);
    if(elem_from_insert != NULL){
        free(new_frame);
        new_frame = hash_entry(elem_from_insert, struct frame, elem_in_hash);
    }
    new_frame->create_time = create_time_global++;
    lock_release(&lock_frame_hash);
    return new_frame;
}
//@3-1 F: frame_free, when palloc-free
bool frame_free(uint8_t* kvaddr){
    ASSERT(is_kernel_vaddr(kvaddr));
    ASSERT(!pg_ofs(kvaddr));

    struct frame frame_help; //#A take care!
    frame_help.kvaddr = kvaddr;

    struct hash_elem *elem_found;
    struct frame *frame_to_del;
    lock_acquire(&lock_frame_hash);
    elem_found = hash_find(&frame_hash, &frame_help.elem_in_hash);
    if(elem_found == NULL){
        lock_release(&lock_frame_hash);
        return false;
    }
    frame_to_del = hash_entry(elem_found, struct frame, elem_in_hash);
    hash_delete(&frame_hash, &frame_to_del->elem_in_hash);
    lock_release(&lock_frame_hash);
    free(frame_to_del);
    return true;
}
//@3-1 F: find_frame
struct frame *find_frame(uint8_t* kvaddr){
    ASSERT(is_kernel_vaddr(kvaddr) && !pg_ofs(kvaddr));

    struct frame frame_help; //#A take care!
    frame_help.kvaddr = kvaddr;

    struct hash_elem *elem_found;
    lock_acquire(&lock_frame_hash);
    elem_found = hash_find(&frame_hash, &frame_help.elem_in_hash);
    lock_release(&lock_frame_hash);

    if(elem_found == NULL)
        return NULL;
    return hash_entry(elem_found, struct frame, elem_in_hash);
}
//@3-3 F: show_upage, debug
void show_upage(){
    struct hash_iterator i;
    // iterate the hash
    printf("==========START==========\n");
    lock_acquire(&lock_frame_hash);
    hash_first(&i, &frame_hash);
    while (hash_next(&i))
    {
        struct frame *f = hash_entry(hash_cur(&i), struct frame, elem_in_hash);
        struct list_elem *e = list_begin(&f->upage_list);
        struct page_info *swp_page = list_entry(e, struct page_info, elem_in_frame);
        struct thread *t = swp_page->allocator;
        printf("uvaddr:%x, dirty:%d\n",swp_page->uvaddr, pagedir_is_dirty(t->pagedir,swp_page->uvaddr));
    }
    lock_release(&lock_frame_hash);
    printf("========== END ==========\n");
    return;
}
//@3-3 F: find_evict
struct frame *find_evict(){
    struct hash_iterator i;
    uint32_t min_create_time = 0xffffffff;
    struct frame *f_evict = NULL;

    // iterate the hash
    lock_acquire(&lock_frame_hash);
    hash_first(&i, &frame_hash);
    while (hash_next(&i))
    {
        struct frame *f = hash_entry(hash_cur(&i), struct frame, elem_in_hash);
        struct list_elem *e = list_begin(&f->upage_list);
        struct page_info *swp_page = list_entry(e, struct page_info, elem_in_frame);
        struct thread *t = swp_page->allocator;
        if(swp_page->writable == true){
            if (f->create_time < min_create_time){
                min_create_time = f->create_time;
                f_evict = f;
            }
        }
    }
    if(f_evict == NULL)
        PANIC("No frame to evict!\n");
    hash_delete(&frame_hash,&f_evict->elem_in_hash);

    lock_release(&lock_frame_hash);
    return f_evict;
}
