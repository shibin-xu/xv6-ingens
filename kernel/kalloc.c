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
void hugekfree(void *pa);
void hugefreerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 nfree;
} kmem;

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 nfree;
} hugekmem;

int initialized;

void
kinit()
{
  initialized = 0;
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSMID);
  initlock(&hugekmem.lock, "hugekmem");
  hugefreerange((void*)PHYSMID, (void*)PHYSTOP);
  initialized = 1;
  printf("free base pages %d MB\n", kmem.nfree * PGSIZE / (1024 * 1024));
  printf("free huge pages %d MB\n", hugekmem.nfree *HPGSIZE / (1024 * 1024));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
hugefreerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + HPGSIZE <= (char*)pa_end; p += HPGSIZE)
    hugekfree(p);
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
  if (initialized)
    memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.nfree++;
  release(&kmem.lock);
}

void
hugekfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % HPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("hugekfree");

  // Fill with junk to catch dangling refs.
  if (initialized)
    memset(pa, 1, HPGSIZE);

  r = (struct run*)pa;

  acquire(&hugekmem.lock);
  r->next = hugekmem.freelist;
  hugekmem.freelist = r;
  hugekmem.nfree++;
  release(&hugekmem.lock);
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
  if(r){
    kmem.freelist = r->next;
    kmem.nfree--;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint64
sys_nfree(void)
{
  return kmem.nfree;
}
