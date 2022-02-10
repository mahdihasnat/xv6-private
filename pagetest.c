#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE (1<<12)
#define PGADDRBIT 12

int
fifo_test(uint max_page)
{
	uint currsize=(uint)sbrk(0);

	for(uint i=currsize;i<max_page * PGSIZE;i+=PGSIZE)
	{
		sbrk(PGSIZE);
	}
	return 0;
}

int
main(int argc, char * argv[]){
	if(fifo_test(30)==0)
		printf(1,"fifo test passed\n");
	else
		printf(1,"fifo test failed\n");

	// uint curr_size=(uint)sbrk(0);
	// printf(1,"current size = %p\n",curr_size);
	// // ddd4
	// uint demand = 0xddd3<<12;
	// printf(1,"demand = %p\n",demand);
	// sbrk(demand+curr_size);
	// printf(1,"current size = %p\n",(uint)sbrk(0));
	exit();
}