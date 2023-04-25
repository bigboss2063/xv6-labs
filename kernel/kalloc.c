// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end, int cpuid);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
void
kinit()
{
  uint64 offset = (PHYSTOP - (uint64)end)/8;
  uint64 left, right;
  left = (uint64) end;
  right = left + offset;
  for (int i = 0; i < NCPU; i++) {
    struct cpu *c = &cpus[i];
    initlock(&c->kmem.lock, "kmem");
    char *p;
    p = (char*)PGROUNDUP(left);
    for(; p + PGSIZE <= (char*)right; p += PGSIZE) {
      struct run *r;

      if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
        panic("kfree");

      // Fill with junk to catch dangling refs.
      memset(p, 1, PGSIZE);
      r = (struct run*)p;
      r->next = c->kmem.freelist;
      c->kmem.freelist = r;
    }
    left = right;
    right = right + offset;
  }
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  struct cpu *c = mycpu();

  acquire(&c->kmem.lock);
  r->next = c->kmem.freelist;
  c->kmem.freelist = r;
  release(&c->kmem.lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc()
{
  push_off();
  struct run *r;
  int cid = cpuid();
  struct cpu *c = &cpus[cid];

  acquire(&c->kmem.lock);
  r = c->kmem.freelist;
  if(r) {
    c->kmem.freelist = r->next;
    release(&c->kmem.lock);
  } else {
    release(&c->kmem.lock);
    for (int i = 0; i < NCPU; i++) {
      if (i == cid) continue;
      struct cpu *nc = &cpus[i];
      acquire(&nc->kmem.lock);
      r = nc->kmem.freelist;
      if (r) {
        acquire(&c->kmem.lock);
        nc->kmem.freelist = r->next;
        release(&c->kmem.lock);
        release(&nc->kmem.lock);
        break;
      } else {
        release(&nc->kmem.lock);
        continue;
      }
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
