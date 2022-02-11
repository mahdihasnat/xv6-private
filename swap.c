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

// read a page from swap
int
swapIn(struct proc *p ,char * buff,uint vpa)
{
	LOGSWAP(cprintf("swapIn: pid %d  vpa %d\n",p->pid,vpa));
	if(readFromSwapFile(p,buff,vpa,PGSIZE)!=PGSIZE)
	{
		LOGSWAP(cprintf(ERROR_STR("swapIn: readFromSwapFile failed\n")));
		return -1;
	}
	return 0;
}

// swap is called either from fork or [userinit|exec]
// this method is called from fork
// upon failure no swap file is created
int
initSwap(struct proc *p)
{
	LOGSWAP(cprintf(DEBUG_STR("initSwap: p->pid %d parent %d pgdir %p\n"), p->pid,p->parent->pid,p->pgdir);)
	AssertPanic(p->parent!=0);	
	// called from forkls
	p->totaPages  = p->parent->totaPages;
	memmove(p->VPA_Memory, p->parent->VPA_Memory, sizeof(p->VPA_Memory));
	#ifdef FIFO_SWAP
		p->q_head = p->parent->q_head;
		p->q_tail = p->parent->q_tail;	
		p->q_size = p->parent->q_size;	
	#endif
	
	if(createSwapFile(p)<0){
		cprintf(ERROR_STR("initSwap: createSwapFile failed\n"));
		return -1;
	}

	// if(p->parent->pid == 1) // parent is init
	// 	return 0;

	// char *buff=0;
	// AssertPanic((buff = kalloc()) != 0);
	
	// for(uint a = 0; a< (p->parent->sz) ; a+=PGSIZE){
		
	// 	if(readFromSwapFile(p->parent, buff, a, PGSIZE)<0){
	// 		cprintf(ERROR_STR("initSwap: readFromParentSwapFile failed\n"));
	// 		removeSwapFile(p);
	// 		return -1;
	// 	}
	// 	if(writeToSwapFile(p, buff, a, PGSIZE)<0){
	// 		cprintf(ERROR_STR("initSwap: writeToSwapFile failed\n"));
	// 		removeSwapFile(p);
	// 		return -1;
	// 	}
	// }
	// if(buff)
	// 	kfree(buff);

	return 0;
}

// src
// dest
// vpa = virtual page address
int
copySwapPage(struct proc* source, struct proc* destination, uint vpa)
{
	LOGSWAP(cprintf(DEBUG_STR("copySwapPage: source->pid %d, destination->pid %d, vpa %p\n"), source->pid, destination->pid, vpa);)
	AssertPanic(source->pid!=0);
	AssertPanic(destination->pid!=0);
	AssertPanic(source->pid!=destination->pid);
	AssertPanic(PTE_FLAGS(vpa) == 0);
	
	char *buff=0;
	AssertPanic((buff = kalloc()) != 0);
	
	if(readFromSwapFile(source, buff, vpa, PGSIZE)<0){
		cprintf(ERROR_STR("copySwapPage: readFromSwapFile failed\n"));
		kfree(buff);
		return -1;
	}
	if(writeToSwapFile(destination, buff, vpa, PGSIZE)<0){
		cprintf(ERROR_STR("copySwapPage: writeToSwapFile failed\n"));
		kfree(buff);
		return -1;
	}
	kfree(buff);
	return 0;
}


int
initFreshSwap(struct proc *p)
{
	LOGSWAP(cprintf(DEBUG_STR("initFreshSwap pid %p\n"),p->pid);)
	
	p->totaPages = 0;
	memset(p->VPA_Memory, 0, sizeof(p->VPA_Memory));
	
#ifdef FIFO_SWAP
	p->q_head = 0;
	p->q_tail = 0;
	p->q_size = 0;
#endif
	if(createSwapFile(p)<0)
		return -1;
	// char * buf = kalloc();
	// for(uint a=0;a<(PGSIZE*10);a+=PGSIZE){
	// 	cprintf(DEBUG_STR("initFreshSwap: writing to swap file %p\n"),a);
	// 	int ret = writeToSwapFile(p, buf, a, PGSIZE);
	// 	if(ret<0){
	// 		cprintf(ERROR_STR("initFreshSwap: writeToSwapFile failed\n"));
	// 		removeSwapFile(p);
	// 		return -1;
	// 	}
	// 	AssertPanic(ret == PGSIZE);
	// }
	// kfree(buf);
	return 0;
}

// restore swap pages to memory, called from exec
int
restoreSwap(struct proc *p)
{
	LOGSWAP(cprintf(DEBUG_STR("restoreSwap pid %p\n"),p->pid);)
	for(int i=0;i<p->sz;i++)
	{
		pte_t * pte = walkpgdir(p->pgdir, (void*)i, 0);
		if(pte == 0)
		{
			cprintf(ERROR_STR("restoreSwap: walkpgdir failed\n"));
			return -1;
		}
		if((*pte)&PTE_PG)
		{
			char * buff = kalloc();
			if(buff == 0)
			{
				cprintf(ERROR_STR("restoreSwap: kalloc failed\n"));
				return -1;
			}
			if(swapIn(p, buff, i)<0)
			{
				cprintf(ERROR_STR("restoreSwap: swapIn failed\n"));
				kfree(buff);
				return -1;
			}
			*pte = (V2P(buff) | PTE_FLAGS(*pte) | PTE_P) ^ PTE_PG;
		}
		else AssertPanic(*pte & PTE_P);
	}
	return 0;
}


int
destroySwap(struct proc *p)
{
	return removeSwapFile(p);
}


// must have pte entry, and pte entry must be valid
// doesnot change pte entry , just write to swap file
// mem is a buffer of size PGSIZE given 
// 0 on success
// -1 on failure
int
forceWriteBack(struct proc *p, uint vpa, char *mem)
{
	AssertPanic(PTE_FLAGS(vpa) == 0);
	LOGSWAP(cprintf(DEBUG_STR("forceWriteBack: pid %p, vpa %p\n"),p->pid, vpa);)
	if(writeToSwapFile(p, mem,vpa , PGSIZE) != PGSIZE)
	{
		cprintf(ERROR_STR("forceWriteBack: writeToSwapFile failed\n"));
		return -1;
	}
	return 0;
}

// swap out, changes in pte
static int
swapOut(struct proc *p, uint vpa)
{
	LOGSWAP(cprintf("swapOut: p->pid %d, vpa %p\n", p->pid, vpa);)
	pte_t * pte = walkpgdir(p->pgdir, (void *)vpa, 0);
	AssertPanic(pte != 0);
	AssertPanic((*pte & PTE_P));
	LOGSWAP(cprintf("swapOut: pte %p *pte \n", pte, *pte);)
	if((*pte) & PTE_D)
	{
		if(writeToSwapFile(p, (char *) P2V( PTE_ADDR(*pte) ), vpa, PGSIZE) != PGSIZE)
		{
			cprintf(ERROR_STR("swapOut: writeToSwapFile failed\n"));
			return -1;
		}
		*pte &= ~PTE_D;
	}

	*pte &= ~PTE_A;
	*pte &= ~PTE_P;
	*pte |= PTE_PG;
	switchuvm(p);

	return 0;
}

// called from allocateuvm after mappages
/***
 * 
 * \arg vpa: virtual page address, Must be Alligned
 * 
 * 
 * */
int
linkNewPage(struct proc *p, uint vpa)
{
	LOGSWAP(cprintf("linkNewPage: p->pid %d vpa %p pgdir %p totalPages %d\n", p->pid, vpa, p->pgdir , p->totaPages);)
	AssertPanic(PTE_FLAGS(vpa) == 0);
#ifdef FIFO_SWAP
	LOGSWAP(cprintf(WARNING_STR("sz:%d head:%d tail:%d\n"),p->q_size,p->q_head,p->q_tail);)
	if(p->q_size == MAX_PSYC_PAGES)
	{
		// queue is full , phy_mem full
		AssertPanic(p->q_head == p->q_tail);

		if(p->totaPages == MAX_TOTAL_PAGES)
		{
			// swap file full
			cprintf(ERROR_STR("linkNewPage: swap file full, totalPage = MAX_TOTAL_PAGE\n"));
			return -1;
		}
		else
		{
			// swap file not full
			// move page to swap file
			LOGSWAP(cprintf(WARNING_STR("linkNewPage: swap file not full, totalPage = %d\n"),p->totaPages);)
			LOGSWAP(cprintf(INFO_STR("prinitng swap info\n"));)
			LOGSWAP(printSwapInfo(p);)
			if(swapOut(p, p->VPA_Memory[p->q_head])<0)
			{
				cprintf(ERROR_STR("linkNewPage: swapOut failed\n"));
				return -1;
			}

			p->VPA_Memory[p->q_head] = vpa;
			p->q_head =p->q_tail = (p->q_head + 1) % MAX_PSYC_PAGES;
			p->q_size++;
		}
	}
	else 
	{

		p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
		// add to tail
		p->VPA_Memory[p->q_tail] = vpa;
		p->q_size++;
	}
#endif
	p->totaPages++;
	cprintf(MAGENTA_STR("linkNewPage: p->pid %d vpa %p pgdir %p totalPages %d\n"), p->pid, vpa, p->pgdir , p->totaPages);
	return 0;
}

// return 0 on success
int
unlinkPage(struct proc *p, uint vpa)
{
	if(p->pid == 1)
		return 0;
	LOGSWAP(cprintf("unlinkPage: p->pid %d vpa %p\n", p->pid, vpa);)
	AssertPanic(PTE_FLAGS(vpa) == 0);

#ifdef FIFO_SWAP
#define CIRCLE_NEXT(x,y) ((x)+1==(y)?0:(x)+1)

	int idx = p->q_head;
	for (uint i = 0; i < p->q_size; i++, idx=CIRCLE_NEXT(idx, MAX_PSYC_PAGES)){
		if (PTE_ADDR(p->VPA_Memory[idx] )== PTE_ADDR(vpa)){
			
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
				AssertPanic(p->q_tail-p->q_head == p->q_size)
			else
				AssertPanic(MAX_PSYC_PAGES - p->q_head + p->q_tail == p->q_size)
			return 0;
		}
	}

#undef CIRCLE_NEXT
#endif
	AssertPanic(p->totaPages > 0);
	p->totaPages--;
	return 0; // not found in memory, may be in hard disk
}

// Return zero on success
int
recoverPageFault(uint va)
{
	return -1;
}