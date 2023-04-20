// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct spinlock pincountlock;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int pincounts[32768];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pincountlock, "pincount");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

int
pa2pincount(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

void
pin(uint64 pa)
{
  pa = PGROUNDUP(pa);
  acquire(&pincountlock);
  if (pincounts[pa2pincount(pa)] > 0){
    pincounts[pa2pincount(pa)]++; 
  }
  release(&pincountlock);
}

void
unpin(uint64 pa)
{
  pa = PGROUNDUP(pa);
  acquire(&pincountlock);
  if (pincounts[pa2pincount(pa)] > 0) {
    pincounts[pa2pincount(pa)]--;
  }
  release(&pincountlock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  unpin((uint64)pa);
  acquire(&pincountlock);
  if (pincounts[pa2pincount((uint64)pa)] != 0){
    release(&pincountlock);
    return;
  }
  release(&pincountlock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    memset((char*)r, 5, PGSIZE); // fill with junk
    uint64 pa = (uint64) r;
    pincounts[pa2pincount(pa)] = 1;
  }
  release(&kmem.lock);
  return (void*)r;
}
