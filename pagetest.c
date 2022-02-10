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

void
fork_test()
{
	int * arr =(int *) sbrk(PGSIZE);
	printf(1,"arr : %p\n",arr);
	arr[10]=10;
	printf(1,"arr[10]=%d\n",arr[10]);
	int x = fork();
	if(x<0)
	{
		printf(1,"fork failed\n");
	}
	if(x==0)
	{
		printf(1,"child sleep 1000\n");
		sleep(1000);
		printf(1,"child arr[10]=%d\n",arr[10]);
		exit();
	}
	else
	{
		sleep(100);
		printf(1,"parent sleep 1000\n");
		sleep(1000);
		wait();
		printf(1," parent arr[10]=%d\n",arr[10]);
		return ;
	}
}

int
main(int argc, char * argv[]){
	if(fifo_test(0)==0)
		printf(1,"fifo test passed\n");
	else
		printf(1,"fifo test failed\n");
	fork_test();
	// uint curr_size=(uint)sbrk(0);
	// printf(1,"current size = %p\n",curr_size);
	// // ddd4
	// uint demand = 0xddd3<<12;
	// printf(1,"demand = %p\n",demand);
	// sbrk(demand+curr_size);
	// printf(1,"current size = %p\n",(uint)sbrk(0));
	exit();
}