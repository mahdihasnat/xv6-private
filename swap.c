#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "swap.h"

// #define LOGSWAP(x) x
#define LOGSWAP(x)


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
	p->size_mem = p->parent->size_mem;	

	#ifdef FIFO_SWAP
		p->q_head = p->parent->q_head;
		p->q_tail = p->parent->q_tail;	
		
	#endif
	#ifdef NFU_SWAP
		// noting to do
	#endif
	
	if(createSwapFile(p)<0){
		cprintf(ERROR_STR("initSwap: createSwapFile failed\n"));
		return -1;
	}
	char *buff=0;
	AssertPanic((buff = kalloc()) != 0);
	for(int i=0;i<NELEM(p->VPA_Swap);i++){
		
		if(!(p->VPA_Swap[i]&SWAP_P))
			continue;

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
	p->size_mem = 0;
#ifdef FIFO_SWAP
	p->q_head = 0;
	p->q_tail = 0;
	
#endif
#ifdef NFU_SWAP
	// noting to do
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
	
	for(int i=0;i<NELEM(p->VPA_Swap);i++)
	{
		// ignore if not present
		if(!((p->VPA_Swap[i])&SWAP_P))
			continue;
		uint vpa = SWAP_ADDR(p->VPA_Swap[i]);
		pte_t *pte = walkpgdir(p->pgdir, (void *)vpa, 0);
		AssertPanic(pte!=0);
		AssertPanic((*pte & PTE_PG));
		AssertPanic(!(*pte & PTE_P));
		int flags = PTE_FLAGS(*pte);
		flags &= ~ PTE_PG;
		flags |= PTE_P;
		char * mem = kalloc();
		cprintf(DEBUG_STR("restoreSwap: read from swap file vpa %x pid: %d\n"), vpa, p->pid);
		if(mem == 0)
		{
			cprintf(ERROR_STR("restoreSwap: kalloc failed\n"));
			return -1;
		}
		uint new_val =  V2P(mem) | flags;
		AssertPanic(V2P(p->pgdir) == rcr3());
		*pte = new_val; 
		lcr3(V2P(p->pgdir));
		AssertPanic(V2P(p->pgdir) == rcr3());
		// cr3 te pgdir update kora lagbe ,  karon etai bortoman process er pgdir e
	}
	return 0;
}


// return available index pages in swap file , -1 in case of none
static int
getFreeSwapPageIndex(struct proc *p)
{
	for(int i=0;i<NELEM(p->VPA_Swap);i++)
	{
		if(!(p->VPA_Swap[i]&SWAP_P))
			return i;
	}
	return -1;
}

// 
static int
moveToSwap(struct proc *p, uint idx_mem,uint idx_swap)
{
	LOGSWAP(cprintf(DEBUG_STR("moveToSwap: idx_mem %d idx_swap %d\n"), idx_mem, idx_swap);)
	AssertPanic(idx_mem < MAX_PSYC_PAGES);
	AssertPanic(idx_swap < MAX_SWAP_PAGES);

	uint vpa = MEM_ADDR(p->VPA_Memory[idx_mem]); // virtual page address
	AssertPanic(PTE_FLAGS(vpa)==0); // last 12 bits are zero

	pte_t *pte ;
	if((pte =(pte_t *) walkpgdir(p->pgdir, (void *)(vpa), 0))== 0) // get pte from pagetable
	{
		cprintf(ERROR_STR("moveToSwap: walkpgdir failed VPA_Memory is dirty\n"));
		return -1;
	}

	char * mem = P2V(PTE_ADDR(*pte)); // get mem address of page
	if(mem == 0)
	{
		cprintf(ERROR_STR("moveToSwap: memory not found in PTE Entry\n"));
		return -1;
	}

	if(writeToSwapFile(p, mem, idx_swap * PGSIZE, PGSIZE)!=PGSIZE)
	{
		cprintf(ERROR_STR("moveToSwap: writeToSwapFile failed\n"));
		return -1;
	}
	p->VPA_Swap[idx_swap] = SWAP_P | vpa;

	uint new_val = (*pte & ~PTE_P)|PTE_PG;
	// *pte &= ~(PTE_P); // unset pte_p
	// *pte |= PTE_PG; // set pte_pg
	AssertPanic(myproc() == p);
	AssertPanic(V2P(p->pgdir) == rcr3());
	// nijer process table update kortese , so tlb upd
	*pte = new_val;
	lcr3(V2P(p->pgdir));
	AssertPanic(V2P(p->pgdir) == rcr3());

	kfree(mem);

	// { sanity test 
	// 	pte_t *Pte = (pte_t *) walkpgdir(p->pgdir, (void *)PTE_ADDR(vpa), 0);
	// 	AssertPanic(pte == Pte);
	// }

	return 0;
}

// move last page to idx & update size
// return 0 on success
// wont use this since we are storing present bit in VPA_Swap
/*
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
*/

// move page from swap to mem , return 0 on success or -1 on error
// clear swap page
int
moveFromSwap(struct proc *p, uint vpa, char * mem){
	AssertPanic(PTE_FLAGS(vpa) == 0);

	for(int i=0;i<NELEM(p->VPA_Swap);i++){
		if(!(p->VPA_Swap[i] & SWAP_P))
			continue;

		if(SWAP_ADDR(p->VPA_Swap[i]) == vpa){
			
			if(readFromSwapFile(p, mem, i*PGSIZE, PGSIZE) != PGSIZE)
			{
				cprintf(ERROR_STR("moveFromSwap: readFromSwapFile failed\n"));
				return -1;
			}
			
			p->VPA_Swap[i] = 0;
			// if(swapFillGap(p, i)!=0)
			// {
			// 	cprintf(ERROR_STR("moveFromSwap: swapFillGap failed\n"));
			// 	return -1;
			// }
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
	LOGSWAP(cprintf(WARNING_STR("sz:%d head:%d tail:%d\n"),p->size_mem,p->q_head,p->q_tail);)
	if(p->size_mem == MAX_PSYC_PAGES)
	{
		LOGSWAP(cprintf(WARNING_STR("swap: queue full\n"));)
		// queue is full , phy_mem full
		AssertPanic(p->q_head == p->q_tail);

		int idx_swap = getFreeSwapPageIndex(p);


		if(idx_swap == -1)
		{
			cprintf(ERROR_STR("linkNewPage: getFreeSwapPageIndex failed | max number of pages reached\n"));
			return -1;
		}
		else
		{
			LOGSWAP(cprintf(INFO_STR("linkNewPage: moving mem %d -> %d swap\n"),p->q_head, idx_swap);)
			// swap file not full
			// move page to swap file
			moveToSwap(p,p->q_head, idx_swap);
			p->VPA_Memory[p->q_tail]= vpa|MEM_P;
			p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
			p->q_head = (p->q_head + 1) % MAX_PSYC_PAGES;
			return 0;
		}
	}
	else 
	{
		// add to tail
		p->VPA_Memory[p->q_tail] = vpa|MEM_P;
		p->q_tail = (p->q_tail + 1) % MAX_PSYC_PAGES;
		p->size_mem++;
		return 0;
	}
#endif
#ifdef NFU_SWAP
	if(p->size_mem == MAX_PSYC_PAGES)
	{
		LOGSWAP(cprintf(WARNING_STR("swap: queue full\n"));)
		// queue is full , phy_mem full

		int idx_swap = getFreeSwapPageIndex(p);
		if(idx_swap == -1)
		{
			cprintf(ERROR_STR("linkNewPage: getFreeSwapPageIndex failed | max number of pages reached\n"));
			return -1;
		}
		// select a page to evict
		// select with lowest counter
		int idx_mem = -1;
		uint mx_count = 0xffffffff;
		for(int i=0;i< MAX_PSYC_PAGES ; i++)
		{
			if(NFU_MEM_COUNTER(p->VPA_Memory[i]) <= mx_count)
			{
				mx_count = NFU_MEM_COUNTER(p->VPA_Memory[i]);
				idx_mem = i;
			}
		}
		AssertPanic(idx_mem != -1);
		LOGSWAP(cprintf(INFO_STR("linkNewPage: moving mem %d -> %d swap\n"),idx_mem, idx_swap);)
		if(moveToSwap(p,idx_mem, idx_swap) != 0)
		{
			cprintf(ERROR_STR("linkNewPage: moveToSwap failed\n"));
			return -1;
		}
		p->VPA_Memory[idx_mem] = vpa | MEM_P ;
		return 0;
	}
	else 
	{
		// add to last element
		p->VPA_Memory[p->size_mem++] = vpa | MEM_P;
		return 0;
	}
#endif

}



// Return zero on success
// Delete the page link from swap file or memory
int
unlinkPage(struct proc *p, uint vpa){
	LOGSWAP(cprintf(INFO_STR("unlinkPage: pid %d vpa %p\n"), p->pid, vpa);)
	AssertPanic(PTE_FLAGS(vpa) == 0);

#ifdef FIFO_SWAP
#define CIRCLE_NEXT(x,y) ((x)+1==(y)?0:(x)+1)

	int idx = p->q_head;
	for (uint i = 0; i < p->size_mem; i++, idx=CIRCLE_NEXT(idx, MAX_PSYC_PAGES)){
		if (MEM_ADDR(p->VPA_Memory[idx] )== (vpa)){
			
			while (i + 1 < p->size_mem){
				int nxt= CIRCLE_NEXT(idx, MAX_PSYC_PAGES);
				p->VPA_Memory[idx] = p->VPA_Memory[nxt];
				idx = nxt;
				i++;
			}
			p->q_tail = idx;
			p->size_mem--;
			if(p->size_mem == 0)
				AssertPanic(p->q_head == p->q_tail)
			else if(p->q_head < p->q_tail)
			{
				LOGSWAP(cprintf("h %d t %d s %d\n",p->q_head,p->q_tail,p->size_mem);)
				AssertPanic(p->q_tail-p->q_head == p->size_mem)
			}
			else
				AssertPanic(MAX_PSYC_PAGES - p->q_head + p->q_tail == p->size_mem)
			return 0;
		}
	}
#endif
#ifdef NFU_SWAP
	AssertPanic(p->size_mem <= MAX_PSYC_PAGES); // sanity check
	for(int i=0;i<p->size_mem;i++)
	{
		if(MEM_ADDR(p->VPA_Memory[i]) == vpa)
		{
			// found
			// move to swap file
			AssertPanic(p->size_mem > 0);
			p->size_mem--;
			p->VPA_Memory[i] = p->VPA_Memory[p->size_mem];
			p->VPA_Memory[p->size_mem] = 0;
			return 0;
		}
	}
#endif

	for(int i=0;i<NELEM(p->VPA_Swap);i++){
		if(!(p->VPA_Swap[i]&SWAP_P))
			continue;
		if(SWAP_ADDR(p->VPA_Swap[i]) == (vpa)){
			// swapFillGap(p,i);
			p->VPA_Swap[i] = 0;
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
	if(mem == 0)
	{
		cprintf(ERROR_STR("recoverPageFault: kalloc failed\n"));
		return -1;
	}
	int flags = PTE_FLAGS(*pte);
	if((moveFromSwap(p, vpa, mem)) < 0){
		cprintf(ERROR_STR("recoverPageFault: moveFromSwap failed | swap file dirty\n"));
		kfree(mem);
		return -1;
	}
	AssertPanic(flags & PTE_PG);
	AssertPanic(!(flags & PTE_P));
	// clear PTE_PG 
	flags &= ~PTE_PG;
	// set PTE_P
	flags |= PTE_P;
	
	AssertPanic(rcr3() == V2P(p->pgdir));
	// nijer page table , so joto taratari somvob cr3 update kore tlb thik korte hobe
	*pte = V2P(mem)| flags;
	lcr3(V2P(p->pgdir));

	AssertPanic(rcr3() == V2P(p->pgdir));
	
	return 0;
}


#ifdef NFU_SWAP
// update conunter of pages
void
nfu_Increment_Counter(struct proc *p)
{
	if(p==0)
		return;
	// LOGSWAP(cprintf("nfu_Increment_Counter: pid %d\n", p->pid);)
	for(uint i=0;i<p->size_mem;i++)
	{
		uint vpa = MEM_ADDR(p->VPA_Memory[i]);
		pte_t *pte = walkpgdir(p->pgdir,(char *) vpa, 0);
		AssertPanic(pte != 0);
		if(*pte & PTE_A)
		{
			LOGSWAP(cprintf(MAGENTA_STR("nfu_Increment_Counter: pid %d vpa %p\n"), p->pid, vpa);)
			uint cnt = NFU_MEM_COUNTER(p->VPA_Memory[i]);
			cnt++;
			p->VPA_Memory[i] = vpa | MEM_P | NFU_MEM_COUNTER(cnt);
			pte_t *pte = walkpgdir(p->pgdir,(char *) vpa, 0);
			*pte &= ~PTE_A; // clear PTE_A
			// cr3 reg change kora lagbe na , karon tlb te ig PTE_A use kore na
			
		}
	}
}
#endif