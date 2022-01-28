#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H
#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
//@4-2 Global-Val
#define INODE_L1_SIZE 16
#define INODE_L2_SIZE 144
#define FILE_MAX_SECTORS 16384
struct bitmap;

void inode_init (void);
//@4-4 C: IC.d.ver
bool inode_create (block_sector_t, off_t, bool, block_sector_t dir_in);

struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

//@4-2 Type-Declare
struct inode_disk;
struct inode_mem;
struct inode;
//@4-2 F: lv1_allocate
uint32_t lv1_allocate(size_t sectors, struct inode_mem *inode_mem);
//@4-2 F: lv2_allocate
uint32_t lv2_allocate(size_t sectors, struct inode_mem *inode_mem);
//@4-2 F: lv3_allocate
uint32_t lv3_allocate(size_t sectors, struct inode_mem *inode_mem);
//@4-2 T: inode_sector_allocate
bool inode_sector_allocate(size_t sectors, struct inode_mem *inode_mem);
//@4-2 F: lv1_free
uint32_t lv1_free(struct inode_mem *inode_mem);
//@4-2 F: lv2_free
uint32_t lv2_free(struct inode_mem *inode_mem);
//@4-2 F: lv3_free
uint32_t lv3_free(struct inode_mem *inode_mem);
//@4-2 T: inode_sector_free
bool inode_sector_free(struct inode *inode);
//@4-2 F: inode_data_read_up
void inode_data_read_up(block_sector_t, struct inode_mem *);
//@4-2 F: inode_data_write_down
void inode_data_write_down(block_sector_t, struct inode_mem *);

//@4-4 F: inode_get_dir_in
block_sector_t inode_get_dir_in(const struct inode *inode);
//@4-4 F: inode_is_dir
bool inode_is_dir(const struct inode *inode);
//@4-4 F: inode_get_open_cnt
int inode_get_open_cnt (const struct inode *inode);
#endif /* filesys/inode.h */

