#include "types.h"
#include "stat.h"
#include "user.h"

void testcfs()
{
	int parent = getpid();
	int child;
	int i;
	double x = 0, z;
	if((child = fork()) == 0) { // child
		setnice(parent, 39);		// if you set parent's priority lower than child, 
								// 2nd ps will only printout parent process,
								// since child finished its job earlier & exit
		for(i = 0; i < 3000; i++){
			for ( z = 0; z < 300000.0; z += 0.1 )
				x =  x + 3.14 * 89.64;
		}
		ps(0);
		exit();
	} else {	
		setnice(child, 39);	  //parent
		for(i = 0; i < 3000; i++){
			for ( z = 0; z < 300000.0; z += 0.1 )
				x =  x + 3.14 * 89.64;
		}
		ps(0);
		wait();
	}
}
int main(int argc, char **argv)
{
		printf(1, "=== TEST START ===\n");
		testcfs();
		printf(1, "=== TEST   END ===\n");
		exit();
}
