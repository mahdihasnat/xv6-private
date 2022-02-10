#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


// swap is called either from fork or [userinit|exec]
// this method is called from fork
int
initSwap(struct proc *p)
{
	cprintf("initSwap: p->pid %d\n", p->pid);
	AssertPanic(p->parent!=0);	
	// called from fork
	memmove(p->VPN_Swap, p->parent->VPN_Swap, sizeof(p->VPN_Swap));
	memmove(p->VPN_Memory, p->parent->VPN_Memory, sizeof(p->VPN_Memory));
	p->swapSize = p->parent->swapSize;
	#ifdef FIFO_SWAP
		p->q_head = p->parent->q_head;
		p->q_tail = p->parent->q_tail;	
		p->q_size = p->parent->q_size;	
	#endif
	
	if(createSwapFile(p)<0)
		return -1;
	for(int i=0;i<p->swapSize;i++){
		char *buff;
		AssertPanic((buff = kalloc()) != 0);
		if(readFromSwapFile(p->parent, buff, i *PGSIZE, PGSIZE)<0)
			return -1;
		if(writeToSwapFile(p, buff, i *PGSIZE, PGSIZE)<0)
			return -1;
		kfree(buff);
	}
	return 0;
}


int
initFreshSwap(struct proc *p)
{
	cprintf("initFreshSwap\n");
	memset(p->VPN_Swap, 0, sizeof(p->VPN_Swap));
	memset(p->VPN_Memory, 0, sizeof(p->VPN_Memory));
	p->swapSize = 0;
#ifdef FIFO_SWAP
	p->q_head = 0;
	p->q_tail = 0;
	p->q_size = 0;
#endif
	return createSwapFile(p);
}


int
destroySwap(struct proc *p)
{
	return removeSwapFile(p);
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
	pte_t *pte ;
	AssertPanic((pte =(pte_t *) walkpgdir(p->pgdir, (void *)vpn, 0))!= 0);
	*pte &= ~(PTE_P|PTE_PG);
	char * mem = P2V(PTE_ADDR(*pte));
	cprintf(INFO_STR("moveToSwap: %d %d %p %p  swsz:%d\n"), p->pid, idx, vpn, mem, p->swapSize);
	AssertPanic(writeToSwapFile(p,mem,(p->swapSize)*PGSIZE , PGSIZE) == PGSIZE);
	kfree(mem);
}


// Two ways to add pages
// copyuvm in fork : copy the parent's page table , same as parent swap state [initSwap]
// allocateuvm from exec or growproc : allocate new page table , need to work on swap file

// link new page is called after mappages ,from allocateuvm
// return 0 on success
int
linkNewPage(struct proc *p, uint vpn)
{
	cprintf(INFO_STR("linkNewPage: %d %p\n"), p->pid, vpn);
	#ifdef FIFO_SWAP
		cprintf(WARNING_STR("sz:%d head:%d tail:%d\n"),p->q_size,p->q_head,p->q_tail);
		if(p->q_size == MAX_PSYC_PAGES)
		{
			
			// queue is full , phy_mem full
			AssertPanic(p->q_head == p->q_tail);

			if(p->swapSize +  MAX_PSYC_PAGES == MAX_TOTAL_PAGES)
			{
				// swap file full
				LOG("process exceeded max  page limit");
				return -1;
			}
			else
			{
				// swap file not full
				// move page to swap file
				moveToSwap(p,p->q_head);
				p->VPN_Memory[p->q_head]= vpn;
				p->q_head =p->q_tail = (p->q_head + 1) % MAX_PSYC_PAGES;
				p->q_size++;
				return 0;
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

#define CIRCLE_NEXT(x,y) ((x)+1==(y)?0:(x)+1)

// Return zero on success
// Delete the page link from swap file or ememory
int
unlinkPage(struct proc *p, uint vpn){
	cprintf(INFO_STR("unlinkPage: %d %p\n"), p->pid, vpn);
#ifdef FIFO_SWAP
	int idx = p->q_head;
	for (uint i = 0; i < p->q_size; i++, idx=CIRCLE_NEXT(idx, MAX_PSYC_PAGES)){
		if (p->VPN_Memory[idx] == vpn){
			
			while (i + 1 < p->q_size){
				int nxt= CIRCLE_NEXT(idx, MAX_PSYC_PAGES);
				p->VPN_Memory[idx] = p->VPN_Memory[nxt];
				idx = nxt;
				i++;
			}
			p->q_tail = idx;
			p->q_size--;
			if(p->q_size == 0)
				AssertPanic(p->q_head == p->q_tail)
			else if(p->q_head < p->q_tail)
				AssertPanic(p->q_tail-p->q_head == p->q_size)
			else
				AssertPanic(MAX_PSYC_PAGES - p->q_head + p->q_tail == p->q_size)
			return 0;
		}
	}
#endif
	for(int i=0;i<p->swapSize;i++){
		if(p->VPN_Swap[i] == vpn){
			if(i + 1 == p->swapSize ){
				p->swapSize--;
				return 0;
			}
			char * buff = kalloc();
			AssertPanic(readFromSwapFile(p, buff, (p->swapSize -1) * PGSIZE, PGSIZE) == PGSIZE);
			AssertPanic(writeToSwapFile(p, buff, i * PGSIZE, PGSIZE) == PGSIZE);
			kfree(buff);
			p->VPN_Swap[i] = p->VPN_Swap[p->swapSize-1];
			p->swapSize--;
			return 0;
		}
	}

	return -1;
}

#undef CIRCLE_NEXT