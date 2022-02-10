#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


// swap is called either from fork or userinit
// this method is called from fork
int
initSwap(struct proc *p)
{

#ifdef FIFO_SWAP
	p->q_head = 0;
	p->q_tail = 0;
#endif

	AssertPanic(p->parent!=0);
	
	// called from fork
	memmove(p->VPN_Swap, p->parent->VPN_Swap, sizeof(p->VPN_Swap));
	memmove(p->VPN_Memory, p->parent->VPN_Memory, sizeof(p->VPN_Memory));
	#ifdef FIFO_SWAP
		p->q_head = p->parent->q_head;
		p->q_tail = p->parent->q_tail;		
	#endif
	
	return createSwapFile(p);
}

int
destroySwap(struct proc *p)
{
	return removeSwapFile(p);
}


void
initFirstProcessSwap(struct proc *p)
{
	memset(p->VPN_Swap, 0, sizeof(p->VPN_Swap));
}

void
printSwapMetaData(struct proc *p)
{
	
}


void
moveToSwap(struct proc *p, uint idx)
{
	AssertPanic(idx < MAX_PSYC_PAGES);
	uint vpn = p->VPN_Memory[idx];
	pte_t *pte = walkpgdir(p->pgdir, (void *)vpn, 0);
	AssertPanic(pte != 0);
	*pte &= ~(PTE_P|PTE_PG);
	char * mem = P2V(PTE_ADDR(*pte));
	AssertPanic(writeToSwapFile(p,mem,(p->swapSize)<<PGADDRBIT , PGSIZE) == 0);
}


// Two ways to add pages
// copyuvm in fork : copy the parent's page table , same as parent swap state [initSwap]
// allocateuvm from exec or growproc : allocate new page table , need to work on swap file

// link new page is called after mappages ,from allocateuvm
// return 0 on success
int
linkNewPage(struct proc *p, uint vpn)
{
	#ifdef FIFO_SWAP
		
		if(p->q_size == MAX_PSYC_PAGES)
		{
			
			// queue is full , phy_mem full
			AssertPanic(p->q_head == p->q_tail);

			if(p->swapSize +  MAX_PSYC_PAGES == MAX_TOTAL_PAGES)
			{
				// swap file full
				return -1;
			}
			else
			{
				// swap file not full
				// move page to swap file
				writeToSwapFile(p,p->VPN_Memory[p->q_head],p->swapSize
			}
			
			

		}
		else 
		{
			p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
			// add to tail
			p->VPN_Memory[p->q_tail] = vpn;
			p->q_size++;
			return 0;
		}
	#endif
}