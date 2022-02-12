#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_ANONYMOUS 0x1
#define MAP_POPULATE 0x2

int main(int argc, char **argv)
{
        int fd = open("README",O_RDWR);
        printf(1,"fd is %d\n",fd);
        printf(1,"FIRST freemem now is %d\n",freemem());
		//char* ptr = malloc(4096);
		char* test = (char *)mmap(0, 4096, PROT_READ|PROT_WRITE,MAP_POPULATE, fd, 0);
		//printf(1, "@@@@@@@@@@@@@@@@@@@@@@@first@@@@@@\n%s\n", test);
        test[2286]=0;
        //printf(1,"@@@@@@@@@@@@@@@@@@@@@@@first@@@@@@\n%s\n", test);
        printf(1,"!!finish\n");

        printf(1,"SECOND freemem now is %d\n",freemem());
        char* test2 = (char*)mmap(4096, 4096, PROT_READ,MAP_ANONYMOUS, -1, 0);
        printf(1,"@@@@@@@@@@@@@@@@@@@@second@@@@@@@\n%s\n",test2);
        printf(1,"!!finish\n");
 
        printf(1,"THIRD freemem now is %d\n",freemem());
        char* test3 = (char*)mmap(8192,8192,PROT_READ|PROT_WRITE,0,fd,0);
        test3[2286]=0;
	//printf(1, "pass\n%s\n", test3);
        //printf(1,"@@@@@@@@@@@@@@@@@@@@@third@@@@@@@\n%s\n",test3);
        printf(1,"!!finish\n");

        printf(1,"FOURTH freemem now is %d\n",freemem());
        char* test4 = (char*)mmap(16384,4096,PROT_READ|PROT_WRITE,MAP_POPULATE|MAP_ANONYMOUS,-1,0);
        test4[2286]=0;
        //printf(1,"@@@@@@@@@@@@@@@@@@@@@@forth@@@@@@@\n%s\n",test4);
        printf(1,"!!finish\n");

        printf(1,"LAST freemem now is %d\n",freemem());
		printf(1, "%x %x %x %x\nGO to munmap!\n", (uint)test, (uint)test2, (uint)test3, (uint)test4);

		int f;
		if((f=fork())==0){
			printf(1, "CHILD START\n");
			int x;
			int base = 0x40000000;
			printf(1,"first#################################\n%s\n",test);
			x = munmap(0+base);
			printf(1,"0: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			printf(1,"second#################################\n%s\n",test2);
			x = munmap(4096+base);
			printf(1,"4096: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			printf(1,"third#################################\n%s\n",test3);
			x = munmap(8192+base);
			printf(1,"8192: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			printf(1,"fourth#################################\n%s\n",test4);
			x = munmap(16384+base);
			printf(1,"16384: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
			
			exit();
			return 0;
		}
		else{
			printf(1, "PARENT START\n");
			int x;
			int base = 0x40000000;
			x = munmap(0+base);
			printf(1,"0: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			//printf(1,"#################################\n%s\n",test);
			x = munmap(8192+base);
			printf(1,"8192: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			//printf(1,"#################################\n%s\n",test3);
			x = munmap(16384+base);
			printf(1,"16384: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			//printf(1,"#################################\n%s\n",test4);
			x = munmap(4096+base);
			printf(1,"4096: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			//printf(1,"#################################\n%s\n",test2);
			wait();
		}
		printf(1,"Lastly freemem now is %d\n",freemem());
        close(fd);
		exit();
        return 0;
}
