#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
	int startTicks = uptime();
	int x = fork();
	if(x<0)
	{
		printf(1,"fork failed\n");
		exit();
	}
	else if(x==0)
	{
		if(argc == 1)
			exit();
		exec(argv[1] , argv+1);
	}
	else
	{
		wait();
		int stopTicks = uptime();
		printf(1,"\ntime: %d ticks\n",stopTicks-startTicks);
		exit();
	}
}