#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE (1<<12)
#define MX (1<<31)

#define readTestPrint(x) {\
	printf(1,"address = %p ,value =%c\n",x,*(char *)(x));\
}

#define readTest(x) {\
	volatile char t = *(char *)(x);\
	t++;\
}

int
main(int argc, char * argv[]){
	
	for(uint offset=0;offset<PGSIZE;offset++){
		readTest(offset);
	}
	exit();
}