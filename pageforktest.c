#include "types.h"
#include "stat.h"
#include "user.h"
 
#define PGSIZE (1<<12)
#define PGADDRBIT 12

int init_page;

void init_mem(uint max_page)
{
	uint currsize=(uint)sbrk(0);
	int curr_page = currsize >> PGADDRBIT;
	init_page = curr_page;
	// printf(1,"curr_page : %d\n",curr_page);
	if(curr_page>max_page)
	{
		printf(1," errorr curr_page:%d > max_page:%d\n",curr_page,max_page);
		exit();
	}
	int new_page = max_page - curr_page;
	// printf(1,"new_page : %d\n",new_page);
	for(int i=0;i<new_page;i++)
	{
		if(sbrk(PGSIZE) == (void*)-1)
		{
			printf(1,"sbrk error\n");
			return;
		}
	}

}

void write_mem(uint max_page,char c)
{
	for(int i=init_page;i<max_page;i++)
	{
		for(int j=0;j<PGSIZE;j++)
		{
			 *(char *)(i*PGSIZE+j) = c + j*PGSIZE + i;
		}
	}
}

int check_mem(uint max_page,char c)
{
	for(int i=init_page;i<max_page;i++)
	{
		for(int j=0;j<PGSIZE;j++)
		{
			if(*(char *)(i*PGSIZE+j) != ((char)(c + j*PGSIZE + i)))
			{
				return -1;
			}
		}
	}
	return 0;
}

void release_mem(int max_page)
{
	for(int i=init_page;i<max_page;i++)
	{
		sbrk(-PGSIZE);
	}
}

int
main(int argc, char * argv[]){
	int mx_page = atoi(argv[1]);
	init_mem(mx_page);
	write_mem(mx_page,'a');
	int x = fork();
	if(x<0)
	{
		printf(1,"fork failed\n");
	}
	if(x==0)
	{
		if(check_mem(mx_page,'a') == -1)
		{
			printf(1,"child check memory failed\n");
			exit();
		}
		release_mem(mx_page);
		init_mem(mx_page);
		write_mem(mx_page,'b');
		if(check_mem(mx_page,'b') == -1)
		{
			printf(1,"child check memory failed\n");
			exit();
		}
		printf(1,"child mem test success\n");
		exit();
	}
	else 
	{
		wait();
		if(check_mem(mx_page,'a') == -1)
		{
			printf(1,"parent check memory failed\n");
			exit();
		}
		release_mem(mx_page);
		init_mem(mx_page);
		write_mem(mx_page,'c');
		if(check_mem(mx_page,'c') == -1)
		{
			printf(1,"parent check memory failed\n");
			exit();
		}
		printf(1,"parent mem test success\n");
		exit();
	}
}