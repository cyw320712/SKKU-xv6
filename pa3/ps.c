#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char *argv[]){
    int num;
    if(argc==2) {
        num = atoi(argv[1]);
        ps(num);
    }
    else ps(0);
    exit();
}