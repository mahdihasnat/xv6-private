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
	
	for(uint i=currsize;i<max_page * PGSIZE;i+=PGSIZE)
	{
		sbrk(-PGSIZE);
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
		printf(1,"child sleep 500\n");
		sleep(500);
		printf(1,"child arr[10]=%d\n",arr[10]);
		exit();
	}
	else
	{
		sleep(100);
		printf(1,"parent sleep 500\n");
		sleep(500);
		wait();
		printf(1," parent arr[10]=%d\n",arr[10]);
		sbrk(PGSIZE);
		return ;
	}
}

void
fork_test2()
{
	int * arr =(int *) sbrk(PGSIZE);
	int x = fork();
	if(x<0)
	{
		printf(1,"fork failed\n");
		return ;
	}
	int a=31;
	if(x==0)
	{
		// child
		a=1e9+7;
	}
	for(int i=0;i<PGSIZE/sizeof(int);i++)
	{
		arr[i]=a * i;
	}

	sleep(100);
	int ok = 1;
	for(int i=0;i<PGSIZE/sizeof(int);i++)
	{
		if(arr[i]!=a*i)
		{
			ok=0;
			break;
		}
	}
	if(x!=0)
		sleep(100);
	if(ok)
	{
		printf(1,"fork test ok\n");
	}
	else
	{
		printf(1,"fork test failed\n");
	}
	if(x==0)
		exit();
	else 
		wait();

}


int
main(int argc, char * argv[]){
	if(fifo_test(10)==0)
		printf(1,"fifo test passed\n");
	else
		printf(1,"fifo test failed\n");
	fork_test();
	fork_test2();
	// uint curr_size=(uint)sbrk(0);
	// printf(1,"current size = %p\n",curr_size);
	// // ddd4
	// uint demand = 0xddd3<<12;
	// printf(1,"demand = %p\n",demand);
	// sbrk(demand+curr_size);
	// printf(1,"current size = %p\n",(uint)sbrk(0));
	exit();
}