#include "types.h"
#include "stat.h"
#include "user.h"
int
main(int argc, char *args[])
{
    int i=0;
    int pid = fork();
    if(pid == 0) {
        for(i =0; i<100; i++ ) { 
            sleep(1);
            printf(0,"child\n");
            yield();
        }
    }
    else {
        for(i =0; i<100; i++) {
            yield();
            printf(0,"parent\n");
            sleep(1);
        }
        wait();    
    }
    exit();
    return 0;
}