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
	memmove(p->VPA_Swap, p->parent->VPA_Swap, sizeof(p->VPA_Swap));
	memmove(p->VPA_Memory, p->parent->VPA_Memory, sizeof(p->VPA_Memory));
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
	memset(p->VPA_Swap, 0, sizeof(p->VPA_Swap));
	memset(p->VPA_Memory, 0, sizeof(p->VPA_Memory));
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

// restore swap pages to memory, called from exec
int
restoreSwap(struct proc *p)
{
	LOGSWAP(cprintf(DEBUG_STR("restoreSwap pid %d\n"),p->pid);)
	
	for(int i=0;i<p->swapSize;i++)
	{
		uint vpa = p->VPA_Swap[i];
		AssertPanic(vpa!=0);
		pte_t *pte = walkpgdir(p->pgdir, (void *)vpa, 0);
		AssertPanic(pte!=0);
		AssertPanic((*pte & PTE_PG));
		AssertPanic(!(*pte & PTE_P));
		int flags = PTE_FLAGS(*pte);
		flags |= ~ PTE_PG;
		flags |= PTE_P;
		char * mem = kalloc();
		if(mem == 0)
		{
			cprintf(ERROR_STR("restoreSwap: kalloc failed\n"));
			return -1;
		}
		*pte = V2P(mem) | flags;
	}
	return 0;
}



void
moveToSwap(struct proc *p, uint idx)
{
	LOGSWAP(cprintf("moveToSwap: p->pid %d idx %d\n", p->pid, idx);)
	AssertPanic(idx < MAX_PSYC_PAGES);
	uint vpa = p->VPA_Memory[idx];
	AssertPanic(PTE_FLAGS(vpa)==0);
	pte_t *pte ;
	AssertPanic((pte =(pte_t *) walkpgdir(p->pgdir, (void *)(vpa), 0))!= 0); // get pte from pagetable
	cprintf("moveto swap pte %x *pte %p\n",pte, *pte);
	*pte &= ~(PTE_P); // unset pte_p
	*pte |= PTE_PG; // set pte_pg
	// {
	// 	pte_t *Pte = (pte_t *) walkpgdir(p->pgdir, (void *)PTE_ADDR(vpa), 0);
	// 	AssertPanic(pte == Pte);
	// }
	char * mem = P2V(PTE_ADDR(*pte)); // get mem address of page 
	cprintf(INFO_STR("moveToSwap: pid %d idx %d vpa %p %p  swsz:%d\n"), p->pid, idx, vpa, mem, p->swapSize);
	AssertPanic(writeToSwapFile(p,mem,(p->swapSize)*PGSIZE , PGSIZE) == PGSIZE); // write to swap file
	kfree(mem); // free memory from memory
	p->VPA_Swap[p->swapSize] = vpa; // set vpa  of page entry in swap index
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
	p->VPA_Swap[idx] = p->VPA_Swap[p->swapSize];
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
		if((p->VPA_Swap[i]) == vpa){
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
				p->VPA_Memory[p->q_tail]= vpa;
				p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
				p->q_head = (p->q_head + 1) % MAX_PSYC_PAGES;
				return 0;
			}
		}
		else 
		{
			
			// add to tail
			p->VPA_Memory[p->q_tail] = vpa;
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
		if ((p->VPA_Memory[idx] )== (vpa)){
			
			while (i + 1 < p->q_size){
				int nxt= CIRCLE_NEXT(idx, MAX_PSYC_PAGES);
				p->VPA_Memory[idx] = p->VPA_Memory[nxt];
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
		if((p->VPA_Swap[i]) == (vpa)){
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
	// switchuvm(p);
	return 0;
}