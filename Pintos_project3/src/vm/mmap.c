//@3-4 #include
#include "vm/mmap.h"
#include "vm/sup_table.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "list.h"
#include <stdbool.h>
#include <stdint.h>
//@3-4 #include
#include "userprog/syscall.h"
#include "string.h"
#include <debug.h>
//@3-4 F: mmap_very_init
void mmap_very_init(){
    mid_global = 0;
}
//@3-4 F: file_map_create
mapid_t file_map_create(int fd, void *addr){
    if(fd == 0 || fd == 1)
        return -1;
    struct thread *cur = thread_current();
    if((addr < cur->data_seg_bound && addr >= 0) 
            || addr >= STACK_LOW_BOUND || pg_ofs(addr) != 0)
        return -1;
    struct fd_struct *fds = find_fd_struct(fd);
    if(fds == NULL)
        return -1;
    int len = file_length(fds->file);
    if(len <= 0)
        return -1;
    
    //#A new inode
    lock_acquire(&file_lock);
    struct file *file_re = file_reopen(fds->file);
    lock_release(&file_lock);
    if(file_re == NULL)
        return -1;
    //#A create-struct
    struct file_map *new_map = malloc(sizeof(struct file_map));
    if(new_map == NULL)
        return -1;
    new_map->mid = mid_global++;
    new_map->len = len;
    new_map->addr_start = addr;
    new_map->file_mapped = file_re;
    list_push_back(&cur->mmap_list, &new_map->elem_in_thread);
    //#A create-pages
    uint8_t* first_page = addr;
    uint8_t* last_page = pg_round_down(addr + len);
    //#A check no-overlap
    for(uint8_t* page_check = first_page; page_check <= last_page; 
            page_check += PGSIZE){
        if(find_page_info(page_check, cur) != NULL)
            return -1;
    }
    //#A empty-lazy
    uint8_t *page_aloc = first_page;
    struct page_info *info_aloc;

    while(page_aloc <= last_page){
        info_aloc = page_info_create(page_aloc, NULL, true);
        //ASSERT(info_aloc);
        info_aloc->map = new_map;
        page_aloc += PGSIZE;
    }
    return new_map->mid;
}
//@3-4 F: file_page_read
bool file_page_read(struct page_info *page_i, uint8_t *kpage){
    uint8_t *upage = page_i->uvaddr;
    struct file_map *map = page_i->map;
    uint8_t *first_page = map->addr_start;
    uint8_t *last_page = pg_round_down(first_page + map->len);
    off_t read_bytes;
    if(upage == last_page)
        read_bytes = map->len % PGSIZE;
    else
        read_bytes = PGSIZE;
    off_t offset = upage - first_page;
    //lock_acquire(&file_lock);
    file_read_at(map->file_mapped, kpage, read_bytes, offset);
    //lock_release(&file_lock);
    size_t actual;
    if(upage == last_page)
        memset(kpage + read_bytes, 0, PGSIZE - read_bytes);
    if(actual != read_bytes)
        return -1;
    return true;
}
//@3-4 F: file_page_write
bool file_page_write(struct page_info *page_i, uint8_t *kpage){
    uint8_t *upage = page_i->uvaddr;
    struct file_map *map = page_i->map;
    uint8_t *first_page = map->addr_start;
    uint8_t *last_page = pg_round_down(first_page + map->len);
    off_t write_bytes;
    if(upage == last_page)
        write_bytes = map->len % PGSIZE;
    else
        write_bytes = PGSIZE;
    off_t offset = upage - first_page;
    size_t actual;
    lock_acquire(&file_lock);
    actual = file_write_at(map->file_mapped, kpage, write_bytes, offset);
    lock_release(&file_lock);
    if(actual != write_bytes)
        return -1;
    return true;
}

//@3-4 F: file_map_free
bool file_map_free(mapid_t mid)
{
    struct file_map* map_2free = find_file_map(mid);
    if(!map_2free)
        return false;
    
    struct thread* cur = thread_current();
    uint8_t *first_page = map_2free->addr_start;
    uint8_t *last_page = pg_round_down(first_page + map_2free->len);
    uint8_t *free_page;
    struct page_info *page_i;

    for(free_page = first_page; free_page <= last_page; free_page += PGSIZE){
        page_i = find_page_info(free_page, cur);
        if(page_i == NULL)
            return false;
        if(page_i->status == SWAP){
            page_info_free(page_i);
            continue;
        }
        struct frame *free_frame = page_i->phy_frame;
        //ASSERT(free_frame);
        uint8_t *free_kpage = free_frame->kvaddr;
        //ASSERT(free_kpage);

        //#A write back
        if(pagedir_is_dirty(cur->pagedir, free_page))
            file_page_write(page_i, free_kpage);
        if(frame_free(free_kpage))
            palloc_free_page(free_kpage); //#A
        page_info_free(page_i);
        //ASSERT(pg_ofs(free_page) == 0);
        pagedir_clear_page(cur->pagedir, free_page); //#A
    }
    lock_acquire(&file_lock);
    file_close(map_2free->file_mapped);
    lock_release(&file_lock);

    list_remove(&map_2free->elem_in_thread);
    free(map_2free);
    return true;
}
//@3-4 F: find_file_map
struct file_map *find_file_map(mapid_t mid)
{
    struct list_elem* map_iter;
    struct thread* cur = thread_current();
    for (map_iter = list_begin (&cur->mmap_list); map_iter != list_end (&cur->mmap_list);
           map_iter = list_next (map_iter))
        {
          struct file_map *tmp = list_entry (map_iter, struct file_map, elem_in_thread);
          if (tmp->mid == mid)
            return tmp;
        }
    return NULL;
}
//@3-4 F: free_mmap
void free_mmap(){
    struct thread *cur = thread_current();
    struct file_map *map_2free;
    struct list_elem* e = list_begin(&cur->mmap_list);
    struct list_elem* e_prev;
    while(e != list_end(&cur->mmap_list)){
        e_prev = e;
        e = list_next(e);
        map_2free = list_entry (e_prev, struct file_map, elem_in_thread);
        file_map_free(map_2free->mid);
    }
    return;
}