#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define LOGSWAP(x) x
// #define LOGSWAP(x)

// swap is called either from fork or [userinit|exec]
// this method is called from fork
// upon failure no swap file is created
int
initSwap(struct proc *p)
{
	LOGSWAP(cprintf("initSwap: p->pid %d\n", p->pid);)
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
	
	if(createSwapFile(p)<0){
		cprintf(ERROR_STR("initSwap: createSwapFile failed\n"));
		return -1;
	}
	char *buff=0;
	AssertPanic((buff = kalloc()) != 0);
	for(int i=0;i<p->swapSize;i++){
		
		if(readFromSwapFile(p->parent, buff, i *PGSIZE, PGSIZE)<0){
			cprintf(ERROR_STR("initSwap: readFromParentSwapFile failed\n"));
			removeSwapFile(p);
			return -1;
		}
		if(writeToSwapFile(p, buff, i *PGSIZE, PGSIZE)<0){
			cprintf(ERROR_STR("initSwap: writeToSwapFile failed\n"));
			removeSwapFile(p);
			return -1;
		}
	}
	if(buff)
		kfree(buff);
	return 0;
}


int
initFreshSwap(struct proc *p)
{
	LOGSWAP(cprintf(INFO_STR("initFreshSwap pid %d\n") , p->pid);)
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
	LOGSWAP(cprintf("moveToSwap: p->pid %d idx %d\n", p->pid, idx);)
	AssertPanic(idx < MAX_PSYC_PAGES);
	uint vpn_perm = p->VPN_Memory[idx];
	pte_t *pte ;
	AssertPanic((pte =(pte_t *) walkpgdir(p->pgdir, (void *)PTE_ADDR(vpn_perm), 0))!= 0); // get pte from pagetable
	vpn_perm = PTE_ADDR(vpn_perm) | PTE_FLAGS(*pte); // get current vpn with permission of page entry
	cprintf("moveto swap pte %x *pte %p\n",pte, *pte);
	*pte &= ~(PTE_P); // unset pte_p
	*pte |= PTE_PG; // set pte_pg
	switchuvm(p);
	{
		pte_t *Pte = (pte_t *) walkpgdir(p->pgdir, (void *)PTE_ADDR(vpn_perm), 0);
		AssertPanic(pte == Pte);
	}
	char * mem = P2V(PTE_ADDR(*pte)); // get mem address of page 
	cprintf(INFO_STR("moveToSwap: %d %d %p %p  swsz:%d\n"), p->pid, idx, vpn_perm, mem, p->swapSize);
	AssertPanic(writeToSwapFile(p,mem,(p->swapSize)*PGSIZE , PGSIZE) == PGSIZE); // write to swap file
	kfree(mem); // free memory from memory
	p->VPN_Swap[p->swapSize] = vpn_perm; // set vpn with permission of page entry in swap index
	p->swapSize++;
}

// move last page to idx & update size
// return 0 on success
int
swapFillGap(struct proc *p,uint idx){
	AssertPanic(p->swapSize > 0);
	AssertPanic(idx < MAX_SWAP_PAGES);
	AssertPanic(p->swapSize > idx);
	p->swapSize--;
	p->VPN_Swap[idx] = p->VPN_Swap[p->swapSize];
	if(idx == p->swapSize)
		return 0;
	char * buff = kalloc();
	if(buff == 0)
	{
		cprintf(ERROR_STR("swapFillGap: kalloc[buffer] failed\n"));
		return -1;
	}
	if(readFromSwapFile(p, buff, p->swapSize*PGSIZE, PGSIZE) != PGSIZE)
	{
		cprintf(ERROR_STR("swapFillGap: readFromSwapFile failed\n"));
		kfree(buff);
		return -1;
	}
	if(writeToSwapFile(p, buff, idx*PGSIZE, PGSIZE) != PGSIZE)
	{
		cprintf(ERROR_STR("swapFillGap: writeToSwapFile failed\n"));
		kfree(buff);
		return -1;
	}
	kfree(buff);
	return 0;
}

// move page from swap to mem , return 0 on success or -1 on error
int
moveFromSwap(struct proc *p, uint vpa, char * mem){
	AssertPanic(PTE_FLAGS(vpa) == 0);
	for(int i=0;i<MAX_SWAP_PAGES;i++){
		if((p->VPN_Swap[i]) == vpa){
			cprintf(INFO_STR("moveFromSwap: %d %d %p %p  swsz:%d\n"), p->pid, i, vpa, mem, p->swapSize);
			if(readFromSwapFile(p, mem, i*PGSIZE, PGSIZE) != PGSIZE)
			{
				cprintf(ERROR_STR("moveFromSwap: readFromSwapFile failed\n"));
				return -1;
			}
			if(swapFillGap(p, i)!=0)
			{
				cprintf(ERROR_STR("moveFromSwap: swapFillGap failed\n"));
				return -1;
			}
			return 0;
		}
	}
	return -1;
}


// Two ways to add pages
// copyuvm in fork : copy the parent's page table , same as parent swap state [initSwap]
// allocateuvm from exec or growproc : allocate new page table , need to work on swap file

// link new page is called after mappages ,from allocateuvm
// return 0 on success
int
linkNewPage(struct proc *p, uint vpa)
{
	LOGSWAP(cprintf(INFO_STR("linkNewPage: pid %d vpa %p\n"), p->pid, vpa);)
	AssertPanic(PTE_FLAGS(vpa) ==0);
	#ifdef FIFO_SWAP
		LOGSWAP(cprintf(WARNING_STR("sz:%d head:%d tail:%d\n"),p->q_size,p->q_head,p->q_tail);)
		if(p->q_size == MAX_PSYC_PAGES)
		{
			LOGSWAP(cprintf(WARNING_STR("swap: queue full\n"));)
			// queue is full , phy_mem full
			AssertPanic(p->q_head == p->q_tail);

			if(p->swapSize +  MAX_PSYC_PAGES == MAX_TOTAL_PAGES)
			{
				LOGSWAP(cprintf(WARNING_STR("swap: swap full\n"));)
				// swap file full
				LOG("process exceeded max  page limit");
				return -1;
			}
			else
			{
				// swap file not full
				// move page to swap file
				moveToSwap(p,p->q_head);
				p->VPN_Memory[p->q_tail]= vpa;
				p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
				p->q_head = (p->q_head + 1) % MAX_PSYC_PAGES;
				p->q_size++;
				return 0;
			}
		}
		else 
		{
			
			// add to tail
			p->VPN_Memory[p->q_tail] = vpa;
			p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
			p->q_size++;
			return 0;
		}
	#endif
}

#define CIRCLE_NEXT(x,y) ((x)+1==(y)?0:(x)+1)

// Return zero on success
// Delete the page link from swap file or memory
int
unlinkPage(struct proc *p, uint vpa){
	LOGSWAP(cprintf(INFO_STR("unlinkPage: pid %d vpa %p\n"), p->pid, vpa);)
	AssertPanic(PTE_FLAGS(vpa) == 0);
#ifdef FIFO_SWAP
	int idx = p->q_head;
	for (uint i = 0; i < p->q_size; i++, idx=CIRCLE_NEXT(idx, MAX_PSYC_PAGES)){
		if ((p->VPN_Memory[idx] )== (vpa)){
			
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
			{
				cprintf("h %d t %d s %d\n",p->q_head,p->q_tail,p->q_size);
				AssertPanic(p->q_tail-p->q_head == p->q_size)
			}
			else
				AssertPanic(MAX_PSYC_PAGES - p->q_head + p->q_tail == p->q_size)
			return 0;
		}
	}
#endif
	for(int i=0;i<p->swapSize;i++){
		if((p->VPN_Swap[i]) == (vpa)){
			swapFillGap(p,i);
			return 0;
		}
	}

	return -1;
}

#undef CIRCLE_NEXT


// Return zero on success
int
recoverPageFault(uint va){
	uint vpa = PGROUNDDOWN(va);
	struct proc *p = myproc();

	pte_t *pte = walkpgdir(p->pgdir,(char *) va, 0);
	
	if(pte == 0)
		return -1;
	
	

	LOGSWAP(cprintf("recoverPageFault: %d  vpa %p  pte %p  *pte %p\n", p->pid, vpa,pte, *pte);)

	if(!(*pte & PTE_PG))
		return -1;
	
	

	char * mem = kalloc();
	AssertPanic(mem != 0);
	int flags = PTE_FLAGS(*pte);
	if((moveFromSwap(p, vpa, mem)) < 0){
		kfree(mem);
		return -1;
	}
	// clear PTE_PG 
	flags &= ~PTE_PG;
	// set PTE_P
	// flags |= PTE_P; // in mappages
	
	if(mappages(p->pgdir, (void*)vpa, PGSIZE, V2P(mem), flags) < 0){
		cprintf(ERROR_STR("recoverPageFault: mappages failed\n"));
		kfree(mem);
		return -1;
	}
	return 0;
}