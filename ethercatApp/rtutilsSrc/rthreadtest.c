#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "rtutils.h"

void cyclic_task(void * usr)
{
    printf("thread create ok\n");
}

int main()
{
    enum { PRIO_LOW = 0, PRIO_HIGH = 60 };
    int prio = PRIO_HIGH;
    if(rtThreadCreate("cyclic", prio, 0, cyclic_task, NULL) == NULL)
    {
        printf("can't create high priority thread, fallback to low priority\n");
        prio = PRIO_LOW;
        assert(rtThreadCreate("cyclic", prio, 0, cyclic_task, NULL) != NULL);
    }
    pause();
    return 0;
}
