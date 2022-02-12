#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char *argv[])
{
    int i, limit = 2, check = 0;
    if(argc==2){
        limit = atoi(argv[1]);
        printf(1, "Argument value: %d\n", limit);
    }
    else {
        printf(1, "No argu, apply Default 2\n");
    }
    
    int cpid, nice, set;

    for(i = 1; i<=limit; i++){
        printf(1, "=====%d====\n", i);
        check = fork();
        if(check < 0){
            printf(1, "Fail to fork()\n");
            exit();
        }
        else if(check == 0){
            cpid = getpid();
            nice = getnice(cpid);
            printf(1, "%d Nice value: %d\n", cpid, nice);
            set = setnice(cpid, nice+i);
            if(set == -1){
                printf(1, "set failed\n");
            }
            else{
                nice = getnice(cpid);
                printf(1, "%d set Success %d\n", cpid, nice);
            }
            ps(cpid);
            exit();
        }
        else 
            wait();
    }
    printf(1, "===================\n");
    check = fork();
    if(check < 0) exit();
    else if(check == 0) printf(1, "zombie\n");
    else exit();
    ps(0);
    exit();
}