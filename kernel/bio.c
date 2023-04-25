// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define HASH_DEV_BLOCKNO(dev, blockno) ((dev + blockno) % NBUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf buckets[NBUCKET];
  struct spinlock bucketlocks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  int i;
  for (i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucketlocks[i], "bcache");
  }

  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.buckets[0].next;
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int key = HASH_DEV_BLOCKNO(dev, blockno);

  acquire(&bcache.bucketlocks[key]);

  // Is the block already cached?
  for (b = bcache.buckets[key].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucketlocks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucketlocks[key]);

  acquire(&bcache.lock);

  for (b = bcache.buckets[key].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      acquire(&bcache.bucketlocks[key]);
      b->refcnt++;
      release(&bcache.bucketlocks[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  int i, bidx = -1, flag = 0;
  struct buf *tmp = 0;
  for (i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucketlocks[i]);
    for (b = &bcache.buckets[i]; b->next; b = b->next) {
      if (b->next->refcnt == 0 && (!tmp || b->next->timestamp > tmp->next->timestamp)) {
          tmp = b;
          flag = 1;
      }
    }
    if (!flag) {
      release(&bcache.bucketlocks[i]);
    } else {
      flag = 0;
      if (bidx != -1) release(&bcache.bucketlocks[bidx]);
      bidx = i;
    }
  }

  if (!tmp) {
    panic("bget: no buffers");
  }

  b = tmp->next;
  tmp->next = b->next;
  release(&bcache.bucketlocks[bidx]);
  
  acquire(&bcache.bucketlocks[key]);
  b->next = bcache.buckets[key].next;
  bcache.buckets[key].next = b;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.bucketlocks[key]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = HASH_DEV_BLOCKNO(b->dev, b->blockno);
  acquire(&bcache.bucketlocks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  
  release(&bcache.bucketlocks[key]);
}

void
bpin(struct buf *b) {
  int key = HASH_DEV_BLOCKNO(b->dev, b->blockno);
  acquire(&bcache.bucketlocks[key]);
  b->refcnt++;
  release(&bcache.bucketlocks[key]);
}

void
bunpin(struct buf *b) {
  int key = HASH_DEV_BLOCKNO(b->dev, b->blockno);
  acquire(&bcache.bucketlocks[key]);
  b->refcnt--;
  release(&bcache.bucketlocks[key]);
}


