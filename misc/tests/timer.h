typedef struct timer_usr timer_usr;
struct timer_usr
{
    int period_ns;
    rtMessageQueueId sink;
};

void timer_task(void * usr);
