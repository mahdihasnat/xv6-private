#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE (1<<12)

int
main(int argc, char * argv[]){
	uint curr_size=(uint)sbrk(0);
	printf(1,"current size = %p\n",curr_size);
	// ddd4
	uint demand = 0xddd3<<12;
	printf(1,"demand = %p\n",demand);
	sbrk(demand+curr_size);
	printf(1,"current size = %p\n",(uint)sbrk(0));
	exit();
}