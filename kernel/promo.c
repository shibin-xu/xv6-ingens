#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

int
promo(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);   // may cause deadlock???
    if(p->state != UNUSED) {
        uint64 sz = p->sz;
        pagetable_t pgtb = p->pagetable;
        release(&p->lock);
        uvmupgrade(pgtb, HPGSIZE, sz);
    } else {
      release(&p->lock);
    }
  }
  return 1;
}

uint64
sys_promo(void)
{
  return promo();
}