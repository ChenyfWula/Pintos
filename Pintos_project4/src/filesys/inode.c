#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
//@4-1 #include
#include "filesys/cache.h"
//@4-2 #include
#include <stdbool.h>
//@4-2 Global-Val
static uint8_t zeros_for_init[BLOCK_SECTOR_SIZE];

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //@4-2 in: inode_disk
    uint32_t sec_num;
    block_sector_t L1_sec[INODE_L1_SIZE]; //#A L1_sec[0] = start, when level_size = 1
    block_sector_t L2_sec;
    block_sector_t L3_sec;
    //@4-4 in: inode_disk
    uint32_t is_directory; //#A as bool, is:1; isnt:0
    block_sector_t dir_in; //#A if is_dir, dir_in = parent-dir (root-par = root)
    uint32_t unused[105];               /* Not used. */
  };//#A 512 Bytes
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

//@4-2 S: inode_mem
struct inode_mem{
    off_t length;                  
    uint32_t sec_num;
    block_sector_t L1_sec[INODE_L1_SIZE];
    block_sector_t L2_sec;
    block_sector_t L3_sec;
    uint32_t is_directory; 
    block_sector_t dir_in; 
  };
/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    //struct inode_disk data;             /* Inode content. */
    //@4-1* in: inode
    off_t write_length;
    //@4-2 in: inode
    struct inode_mem data;
    //@4-3 in: inode
    struct lock inode_lock;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos, bool is_write) 
{ //@4-1* C: B2S.w.ver
  ASSERT (inode != NULL);
  //@4-2 in: byte_to_sector
  if(is_write == false)
    if (pos >= inode->data.length) //#A include length = 0
      return -1;
  else
    if (pos >= inode->write_length) //#A include length = 0
      return -1;

  block_sector_t offest_in_sec = pos / BLOCK_SECTOR_SIZE;
  if(offest_in_sec < INODE_L1_SIZE){
    return inode->data.L1_sec[offest_in_sec];
  }
  else if(offest_in_sec < INODE_L2_SIZE){
    block_sector_t indirect[BLOCK_SECTOR_SIZE / 4];
    block_read(fs_device, inode->data.L2_sec, &indirect);
    return indirect[offest_in_sec - INODE_L1_SIZE];
  }
  else if(offest_in_sec < FILE_MAX_SECTORS){
    block_sector_t ent_num = BLOCK_SECTOR_SIZE / 4;
    block_sector_t temp[BLOCK_SECTOR_SIZE / 4];
    block_read(fs_device, inode->data.L3_sec, &temp);
    block_read(fs_device, temp[(offest_in_sec - INODE_L2_SIZE) / ent_num], 
                &temp);
    return temp[(offest_in_sec - INODE_L2_SIZE) % ent_num];
  }
  return -1;

  // if (pos < inode->data.length)
  //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  // else
  //   return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes; //#A global inode-list
/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  //@4-2 in: inode_init
  memset(zeros_for_init, 0, BLOCK_SECTOR_SIZE);
}

//@4-2 F: lv1_allocate
uint32_t lv1_allocate(size_t sectors, struct inode_mem *inode_mem){
  ASSERT(sectors >= inode_mem->sec_num);
  //#A cond: lv1_allocate no-need 
  if(inode_mem->sec_num >= INODE_L1_SIZE || sectors == inode_mem->sec_num)
    return 0;
  //#A loop aloc sectors
  bool success = true;
  uint32_t data_sec_alloc = 0;
  int i;
  for(i = inode_mem->sec_num; i < sectors && i < INODE_L1_SIZE; i++){
    success = free_map_allocate(1, &inode_mem->L1_sec[i]);
    if(success == false)
      break;
    block_write(fs_device, inode_mem->L1_sec[i], zeros_for_init);
    data_sec_alloc ++;
  }
  //#A tail
  //inode_disk->start = inode_disk->L1_sec[0];
  inode_mem->sec_num = i;
  return data_sec_alloc;
}
//@4-2 F: lv2_allocate
uint32_t lv2_allocate(size_t sectors, struct inode_mem *inode_mem){
  ASSERT(sectors >= inode_mem->sec_num);
  //#A cond: lv2_allocate, no-need
  if(inode_mem->sec_num >= INODE_L2_SIZE || sectors == inode_mem->sec_num)
    return 0;
  
  uint32_t ent_num = BLOCK_SECTOR_SIZE / 4;
  block_sector_t indirect[ent_num];

  bool success = true;
  //#A cond: grow
  if(inode_mem->sec_num == INODE_L1_SIZE){
    success = free_map_allocate(1, &inode_mem->L2_sec);
    if(success == false)
      return 0;
    memset(indirect, 0, BLOCK_SECTOR_SIZE);
  }
  else{ //#A already L2_sec
    block_read(fs_device, inode_mem->L2_sec, indirect);
  }
  //#A loop aloc sectors
  uint32_t data_sec_alloc = 0;
  int i;
  for(i = inode_mem->sec_num; i < sectors && i < INODE_L2_SIZE; i++){
    success = free_map_allocate(1, &indirect[(i - INODE_L1_SIZE) % ent_num]);
    if(success == false)
      break;
    block_write(fs_device, indirect[(i - INODE_L1_SIZE) % ent_num], 
                zeros_for_init);
    data_sec_alloc ++;
  }
  //#A tail
  block_write(fs_device, inode_mem->L2_sec, indirect);
  inode_mem->sec_num = i;
  return data_sec_alloc;
}
//@4-2 F: lv3_allocate
uint32_t lv3_allocate(size_t sectors, struct inode_mem *inode_mem){
  ASSERT(sectors >= inode_mem->sec_num);
  //#A cond: lv3_allocate, no-need
  if(sectors == inode_mem->sec_num)
    return 0;
  
  uint32_t ent_num = BLOCK_SECTOR_SIZE / 4;
  block_sector_t indirect[ent_num];
  block_sector_t in_indirect[ent_num];
  
  bool success = true;
  //#A cond: grow
  if(inode_mem->sec_num == INODE_L2_SIZE){
    success = free_map_allocate(1, &inode_mem->L3_sec);
    if(success = false)
      return 0;
    memset(in_indirect, 0, BLOCK_SECTOR_SIZE);
  }
  else{ //#A already L3_sec
    block_read(fs_device, inode_mem->L3_sec, in_indirect);
  }

  if((inode_mem->sec_num - INODE_L2_SIZE) % ent_num == 0){
    memset(indirect, 0, BLOCK_SECTOR_SIZE);
  }
  else{ //#A start from last used-indirect-sector
    block_read(fs_device, 
      in_indirect[(inode_mem->sec_num - INODE_L2_SIZE) / ent_num], indirect);
  }
  //#A loop aloc sectors
  uint32_t data_sec_alloc = 0;
  int i;
  for(i = inode_mem->sec_num; i < sectors && i < FILE_MAX_SECTORS; i++){
    if((i - INODE_L2_SIZE) % ent_num == 0){ //#A change an indirect-sector
      success = 
        free_map_allocate(1, &in_indirect[(i - INODE_L2_SIZE) / ent_num]);
      if(success == false)
        break;
      if(i != inode_mem->sec_num){
        block_write(fs_device, 
          in_indirect[((i - INODE_L2_SIZE) / ent_num) - 1], indirect);
        memset(indirect, 0, BLOCK_SECTOR_SIZE);
      }  
    }
    success = free_map_allocate(1, &indirect[(i - INODE_L2_SIZE) % ent_num]);
    if(success == false)
      break;
    block_write(fs_device, indirect[(i - INODE_L2_SIZE) % ent_num], 
                zeros_for_init);
    data_sec_alloc ++;
  }
  //#A tail
  block_write(fs_device, //#A NOT (sectors - 1) / ent_num
             in_indirect[(i - 1 - INODE_L2_SIZE) / ent_num], indirect); 
  block_write(fs_device, inode_mem->L3_sec, in_indirect);
  inode_mem->sec_num = i;
  return data_sec_alloc;
}
//@4-2 T: inode_sector_allocate
bool inode_sector_allocate(size_t sectors, struct inode_mem *inode_mem){
  ASSERT(inode_mem->sec_num <= FILE_MAX_SECTORS);
  ASSERT(sectors <= FILE_MAX_SECTORS);
  
  uint32_t data_sec_alloc = 0;
  uint32_t true_alloc = sectors - inode_mem->sec_num;

  data_sec_alloc += lv1_allocate(sectors, inode_mem);
  data_sec_alloc += lv2_allocate(sectors, inode_mem);
  data_sec_alloc += lv3_allocate(sectors, inode_mem); 

  ASSERT(data_sec_alloc == true_alloc);

  return true;
}
//@4-2 F: inode_data_init
void inode_data_read_up(block_sector_t sector, struct inode_mem *inode_mem){
  struct inode_disk disk_inode;
  ASSERT(sizeof disk_inode == BLOCK_SECTOR_SIZE);

  block_read(fs_device, sector, &disk_inode);
  inode_mem->length = disk_inode.length;
  inode_mem->sec_num = disk_inode.sec_num;
  for(int i = 0; i < INODE_L1_SIZE; i++)
    inode_mem->L1_sec[i] = disk_inode.L1_sec[i];
  inode_mem->L2_sec = disk_inode.L2_sec;
  inode_mem->L3_sec = disk_inode.L3_sec;
  inode_mem->is_directory = disk_inode.is_directory;
  inode_mem->dir_in = disk_inode.dir_in;
  return;
}
//@4-2 F: inode_data_write_down
void inode_data_write_down(block_sector_t sector,struct inode_mem *inode_mem){
  struct inode_disk disk_inode;
  ASSERT(sizeof disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode.magic = INODE_MAGIC;

  disk_inode.length = inode_mem->length;
  disk_inode.sec_num = inode_mem->sec_num;
  for(int i = 0; i < INODE_L1_SIZE; i++)
    disk_inode.L1_sec[i] = inode_mem->L1_sec[i];
  disk_inode.L2_sec = inode_mem->L2_sec;
  disk_inode.L3_sec = inode_mem->L3_sec;
  disk_inode.is_directory = inode_mem->is_directory;
  disk_inode.dir_in = inode_mem->dir_in;
  block_write(fs_device, sector, &disk_inode);
  return;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, 
              bool is_dir, block_sector_t dir_in)
{ //@4-4 C: IC.d.ver
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof(struct inode_disk) == BLOCK_SECTOR_SIZE);
  ASSERT(length >= 0);
  
  //@4-2 in: inode_create
  struct inode_mem *inode_mem;
  inode_mem = calloc(1, sizeof *inode_mem);
  if(inode_mem == NULL)
    return false;

  size_t sectors = bytes_to_sectors (length);
  inode_mem->length = length;
  //@4-4 in: inode_create
  inode_mem->is_directory = is_dir;
  inode_mem->dir_in = dir_in;

  inode_mem->sec_num = 0;
  ASSERT(inode_sector_allocate(sectors, inode_mem));
  ASSERT(inode_mem->sec_num == bytes_to_sectors(length));
  inode_data_write_down(sector, inode_mem);

  free(inode_mem);
  
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector) //#A sector in disk
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode); //#A just open_cnt ++
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem); //#A remove in inode_close
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false; //#A delay remove, check in inode_close
  inode_data_read_up(sector, &inode->data);
  //block_read (fs_device, inode->sector, &inode->data); //#A a copy in MEM
  //@4-1* in: inode_open
  inode->write_length = inode->data.length;
  //@4-3 in: inode_open
  lock_init(&inode->inode_lock);

  return inode;
}

/* Reopens and returns INODE. */ //#A use in exist-open
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */ //#A inumber def by its pos in Disk(sector)
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

//@4-2 F: lv1_free
uint32_t lv1_free(struct inode_mem *inode_mem){
  size_t sectors = inode_mem->sec_num;
  if(sectors == 0)
    return 0;
  
  uint32_t data_sec_del = 0;
  for(int i = 0; i < sectors && i < INODE_L1_SIZE; i++){
    free_map_release(inode_mem->L1_sec[i], 1);
    data_sec_del ++;
  }
  return data_sec_del;
}
//@4-2 F: lv2_free
uint32_t lv2_free(struct inode_mem *inode_mem){
  size_t sectors = inode_mem->sec_num;
  if(sectors <= INODE_L1_SIZE)
    return 0;
  block_sector_t indirect[BLOCK_SECTOR_SIZE / 4];
  block_read(fs_device, inode_mem->L2_sec, indirect);

  uint32_t data_sec_del = 0;
  for(int i = INODE_L1_SIZE; i < sectors && i < INODE_L2_SIZE; i++){
    free_map_release(indirect[i - INODE_L1_SIZE], 1);
    data_sec_del ++;
  }
  free_map_release(inode_mem->L2_sec, 1);
  return data_sec_del;
}
//@4-2 F: lv3_free
uint32_t lv3_free(struct inode_mem *inode_mem){
  size_t sectors = inode_mem->sec_num;
  if(sectors <= INODE_L2_SIZE)
    return 0;
  
  block_sector_t in_indirect[BLOCK_SECTOR_SIZE / 4];
  block_read(fs_device, inode_mem->L3_sec, in_indirect);

  block_sector_t indirect[BLOCK_SECTOR_SIZE / 4];

  int ent_num = BLOCK_SECTOR_SIZE / 4;

  uint32_t data_sec_del = 0;
  for(int i = INODE_L2_SIZE; i < sectors && i < FILE_MAX_SECTORS; i++){
    if((i - INODE_L2_SIZE) % ent_num == 0){ //#A change an indirect-sector
      block_read(fs_device, in_indirect[(i - INODE_L2_SIZE) / ent_num], 
                  indirect);
      free_map_release(in_indirect[(i - INODE_L2_SIZE) / ent_num], 1);
    }
    free_map_release(indirect[(i - INODE_L2_SIZE) % ent_num], 1);
    data_sec_del ++;
  }
  free_map_release(inode_mem->L3_sec, 1);
  return data_sec_del;
}
//@4-2 T: inode_sector_free
bool inode_sector_free(struct inode *inode){
  uint32_t true_del = bytes_to_sectors(inode->data.length);
  ASSERT(true_del == inode->data.sec_num);
  uint32_t data_sec_del = 0;
  data_sec_del += lv1_free(&inode->data);
  data_sec_del += lv2_free(&inode->data);
  data_sec_del += lv3_free(&inode->data);
  ASSERT(data_sec_del == true_del);
  return true;
}
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem); //#A NO-LOCK in this func!
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        { 
          free_map_release (inode->sector, 1); //#A inode ifself (bitmap)
          //@4-2 in: inode_close
          inode_sector_free(inode);
          // free_map_release (inode->data.start, //#A data (bitmap)
          //                   bytes_to_sectors (inode->data.length));
        } //#A write in inode_close -> bitmap_write
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{   //#A called in dir_remove
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{   //#A like inode_write_at
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  
  while (size > 0) //#A read > length, sector_idx = -1
    {
      /* Disk sector to read, starting byte offset within sector. */
      //@4-1* C: B2S.w.ver
      block_sector_t sector_idx = byte_to_sector (inode, offset, false); //#A useful
      
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset; //#A <= 0 if offset > .
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;
        //#A inode_left <= sector_left, always
      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0) //#A IMPORTANT
        break;
      //@4-1 in: read_at
      int cidx = get_entry_cache(sector_idx); //#A seem, no "sector" above
      memcpy(buffer + bytes_read, cache[cidx].data + sector_ofs, chunk_size);
      cache[cidx].open_cnt--;
        
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //   }
      // else 
      //   {
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
  
  //@4-3 in: inode_write_at.1
  bool grow = false;
  if(offset + size >= inode->data.length)
    grow = true;
  if(grow == true){
    lock_acquire(&inode->inode_lock);
    size_t dest_sector = bytes_to_sectors(offset + size);
    ASSERT(inode_sector_allocate(dest_sector, &inode->data));
    //@4-1* in: write_at.1
    inode->write_length = offset + size;
  }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      //@4-1* C: B2S.w.ver
      block_sector_t sector_idx = byte_to_sector (inode, offset, true);

      int sector_ofs = offset % BLOCK_SECTOR_SIZE; //#A offset changes in-while
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      //@4-1* in: write_at.2
      off_t inode_left = inode->write_length - offset;

      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;  
      int min_left = inode_left < sector_left ? inode_left : sector_left;
          //#A inode_left <= sector_left, always
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      //@4-1 in: write_at.3
      int cidx = get_entry_cache(sector_idx); //#A seem, no "sector" above
      memcpy(cache[cidx].data + sector_ofs, buffer + bytes_written, chunk_size);
      cache[cidx].dirty = true;
      cache[cidx].open_cnt--;
        
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Write full sector directly to disk. */
      //     block_write (fs_device, sector_idx, buffer + bytes_written);
      //   }   //#A one sector a time
      // else 
      //   {   //#A case for first & last one; make write-data into a sector
      //     /* We need a bounce buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     /* If the sector contains data before or after the chunk
      //        we're writing, then we need to read in the sector
      //        first.  Otherwise we start with a sector of all zeros. */
      //     if (sector_ofs > 0 || chunk_size < sector_left) 
      //       block_read (fs_device, sector_idx, bounce);
      //     else
      //       memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     block_write (fs_device, sector_idx, bounce);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce); //#A in pintos, free(NULL) cause nothing.
  //@4-3 in: inode_write_at.2
  if(grow == true){
    lock_release(&inode->inode_lock);
    //@4-1* in: write_at.4
    inode->data.length = inode->write_length;
    //@4-2 in: inode_write_at
    inode_data_write_down(inode->sector, &inode->data);
  }
    
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{  //#A only called by file_deny_write
  inode->deny_write_cnt++; //#A like our host?
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{   //#A only called by file_allow_write
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

//@4-4 F: inode_get_dir_in
block_sector_t inode_get_dir_in(const struct inode *inode){
  return inode->data.dir_in;
}

//@4-4 F: inode_is_dir
bool inode_is_dir (const struct inode *inode){
  return inode->data.is_directory;
}

int inode_get_open_cnt (const struct inode *inode)
{
  return inode->open_cnt;
}