#include "types.h"
#include "stat.h"
#include "user.h"
int
main(int argc, char *args[])
{
    int ppid = getppid();
    int pid = getpid();
    printf(1, "My pid is %d\n",pid);
    printf(1, "My ppid is %d\n",ppid);
    return 0;
}