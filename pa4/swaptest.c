#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"


int main () {
    char* arr[570];

	int i = 0;
	for (i = 0; i < 570; i++) {
		arr[i] = sbrk(409600);
	    printf(1, "arr[%d]=0x%x\n", i, arr[i]);
        if(i==569) {
            for(int j=0; j<20; j++)
                printf(1, "arr[%d]=%s\n", j, arr[j][0]);
        }
	}

    exit();
}
