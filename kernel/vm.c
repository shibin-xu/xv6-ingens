#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

void print(pagetable_t);

int UPGRADE = 1;
int DOWNGRADE = 0;

/*
 * create a direct-map page table for the kernel and
 * turn on paging. called early, in supervisor mode.
 * the page allocator is already initialized.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface 0
  kvmmap(VIRTION(0), VIRTION(0), PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface 1
  kvmmap(VIRTION(1), VIRTION(1), PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..39 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..12 -- 12 bits of byte offset within the page.
static pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc, int level)
{
  if (va >= MAXVA || level < 0 || level > 2)
    panic("walk");

  for (int i = 2; i > level; i--) {
    pte_t *pte = &pagetable[PX(i, va)];
    if(*pte & PTE_V) {
      if (*pte & PTE_R || *pte & PTE_W || *pte & PTE_X)
        return pte;
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(level, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1, 0)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
hugemappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = HPGROUNDDOWN(va);
  last = HPGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1, 1)) == 0)
      return -1;
    // when upgrade from base page to huge page, *pte could have PTE_V set
    if ((*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X))
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += HPGSIZE;
    pa += HPGSIZE;
  }
  return 0;
}

// Remove mappings from a page table. The mappings in
// the given range must exist. Optionally free the
// physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free)
{
  uint64 a, last, ishuge = 0;
  pte_t *pte;
  uint64 pa;

  if (size == 0)
    return;

  if (((pte = walk(pagetable, va, 0, 1)) != 0) && ((*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X)))
    a = HPGROUNDDOWN(va);
  else
    a = PGROUNDDOWN(va);
  if (((pte = walk(pagetable, va + size - 1, 0, 1)) != 0) && (*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X))
    last = HPGROUNDDOWN(va + size - 1);
  else
    last = PGROUNDDOWN(va + size - 1);
  for (;;){
    if (((pte = walk(pagetable, a, 0, 1)) != 0))
      ishuge = (*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X);

    if (!ishuge && (pte = walk(pagetable, a, 0, 0)) == 0)
      panic("uvmunmap: base page not mapped");
    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    if (do_free){
      pa = PTE2PA(*pte);
      if (ishuge)
        hugekfree((void*)pa);
      else
        kfree((void*)pa);
    }
    *pte = 0;
    if (a == last)
      break;
    if (ishuge) {
      a += HPGSIZE;
      pa += HPGSIZE;
    } else {
      a += PGSIZE;
      pa += PGSIZE;
    }
    if (a > last)
      break;
  }
}

// create an empty user page table.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    panic("uvmcreate: out of memory");
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

uint64
uvmupgrade(pagetable_t pagetable, uint64 start, uint64 end)
{
  char *mem;
  uint64 a, pa;
  pte_t *pte;
  int upgraded;

  // avoid upgrading first 512 base pages since they contain user stack
  a = HPGROUNDDOWN(start);
  for (; a < HPGROUNDDOWN(end); a += HPGSIZE) {
    if ((pte = walk(pagetable, a, 0, 1)) == 0) {
      uvmdealloc(pagetable, start, a);
      upgraded = 0;
      return 0;
    }
    if ((*pte & PTE_R) || (*pte & PTE_W) || (*pte & PTE_X))
      continue;

    mem = hugekalloc();
    if (mem == 0) {
      uvmdealloc(pagetable, start, a);
      upgraded = 0;
      return 0;
    }
    memset(mem, 0, HPGSIZE);
    // after upgrading 512 base pages to 1 huge page, we need to free the pagetable for them
    uint64 p = PTE2PA(*pte);
    // copy from base page to huge page
    for (int i = 0; i < HPGSIZE / PGSIZE; i++) {
      if ((pte = walk(pagetable, a + i*PGSIZE, 0, 0)) == 0)
        panic("uvmupgrade: pte should exist");
      pa = PTE2PA(*pte);
      memmove(mem + i*PGSIZE, (char *)pa, PGSIZE);
    }
    upgraded = 1;
    // free base pages and pagetable
    uvmunmap(pagetable, a, HPGSIZE, 1);
    kfree((void *)p);
    if (hugemappages(pagetable, a, HPGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0) {
      hugekfree(mem);
      uvmdealloc(pagetable, start, a);
      upgraded = 0;
      return 0;
    }
  }

  if (upgraded != 0) {
    sfence_vma();
  }

  return end;
}

uint64
uvmdowngrade(pagetable_t pagetable, uint64 start, uint64 end)
{
  char *mem;
  uint64 a, pa;
  pte_t *pte;

  a = HPGROUNDDOWN(start);
  for (; a < HPGROUNDDOWN(end); a += HPGSIZE) {
    if ((pte = walk(pagetable, a, 0, 1)) == 0) {
      uvmdealloc(pagetable, start, a);
      return 0;
    }
    // check if huge page is mapped
    if (!((*pte & PTE_R) || (*pte & PTE_W) || (*pte & PTE_X)))
      continue;

    // after downgrading 1 huge page to 512 base pages, we need to free the huge page
    pa = PTE2PA(*pte);
    *pte = 0;
    for (int i = 0; i < HPGSIZE / PGSIZE; i++) {
      mem = kalloc();
      memmove(mem, (char *)(pa + i*PGSIZE), PGSIZE);
      if (mappages(pagetable, a + i*PGSIZE, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0) {
        kfree(mem);
        uvmdealloc(pagetable, start, a);
        return 0;
      }
    }
    hugekfree((void *)pa);
  }

  sfence_vma();

  return end;
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a, ishuge = 0;
  pte_t *pte;

  if (newsz < oldsz)
    return oldsz;

  // check if last mapped page is a huge page
  if ((pte = walk(pagetable, oldsz, 0, 1)) != 0)
    ishuge = (*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X);
  if (ishuge)
    oldsz = HPGROUNDUP(oldsz);
  else
    oldsz = PGROUNDUP(oldsz);
  a = oldsz;
  for (; a < newsz; a += PGSIZE){
    mem = kalloc();
    if (mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }

  if (UPGRADE)
    uvmupgrade(pagetable, HPGSIZE, newsz);
  if (DOWNGRADE)
    uvmdowngrade(pagetable, HPGSIZE, newsz);

  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  uint64 newup, olddown, pa, offset, ishuge = 0;
  pte_t *pte;

  if (newsz >= oldsz)
    return oldsz;

  if ((pte = walk(pagetable, newsz, 0, 1)) != 0 && (ishuge = (*pte & PTE_R) | (*pte & PTE_W) | (*pte & PTE_X))) {
    newup = HPGROUNDUP(newsz);
    olddown = HPGROUNDUP(oldsz);
  } else {
    newup = PGROUNDUP(newsz);
    olddown = PGROUNDUP(oldsz);
  }
  if (newup < olddown)
    uvmunmap(pagetable, newup, oldsz - newup, 1);
  if (ishuge && newup - newsz >= PGSIZE) {
    // newsz is within a huge page, and we want to free the base pages within this huge page
    pa = PTE2PA(*pte);
    offset = PGROUNDUP(newsz) % HPGSIZE;
    memset((void *)(pa + offset), 0, HPGSIZE - offset);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
static void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, 0, sz, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
