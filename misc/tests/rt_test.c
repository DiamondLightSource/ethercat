#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <unistd.h>
#include <time.h>
#include <malloc.h>
#include <assert.h>

#include "ecrt.h"

#include "slaves.h"

ec_slave_config_t *sc;

#define Beckhoff_EL4102 0x00000002, 0x10063052
#define EL4102Channel1 0x3001, 1
#define FREQUENCY 2000
#define CLOCK_TO_USE CLOCK_MONOTONIC
#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC +       \
                       (B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)
  
/****************************************************************************/

#define MAX_BINS 10
#define MAX_LATENCY 50.0
float hist[MAX_BINS] = {1.0};

static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
static uint8_t *domain1_pd = NULL;

static int off_aout;

static unsigned int counter = 0;
static unsigned int blink = 0;
static unsigned int sync_ref_counter = 0;
const struct timespec cycletime = {0, PERIOD_NS};

/*****************************************************************************/

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    } else {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

float max_us = 0;

void * cyclic_task(void * usr)
{
    printf("Starting cyclic function.\n");
    struct timespec wakeupTime, endTime;
    // get current time
    clock_gettime(CLOCK_TO_USE, &wakeupTime);
    int blink = 0;
    while(1) 
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        // receive process data
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);
        // calculate new process data
        blink = !blink;
        // write process data
        EC_WRITE_U16(domain1_pd + off_aout, blink ? 0x2000 : 0x0000);
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
        clock_gettime(CLOCK_TO_USE, &endTime);

        double timedel_us = DIFF_NS(wakeupTime, endTime) * 1e-3;
        
        int bin = (int)(timedel_us / MAX_LATENCY * (MAX_BINS-1));
        if(bin < 0)
        {
            bin == 0;
        }
        if(bin >= MAX_BINS)
        {
            bin = MAX_BINS-1;
        }
        hist[bin] += 1.0;
        
        if(timedel_us > max_us)
        {
            max_us = timedel_us;
        }
    }
}

/****************************************************************************/

ec_pdo_entry_reg_t domain1_regs[] = 
{
    {0, 1, Beckhoff_EL4102, EL4102Channel1, &off_aout},
    {0}
};


int main(int argc, char **argv)
{

    assert(mlockall(MCL_CURRENT | MCL_FUTURE) != -1);

    master = ecrt_request_master(0);
    assert(master);
    domain1 = ecrt_master_create_domain(master);
    assert(domain1);

    assert(ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs) == 0);
    assert(ecrt_master_activate(master) == 0);

    domain1_pd = ecrt_domain_data(domain1);
    assert(domain1_pd);

    struct sched_param sched = {0};
    // tuning guide does not recommend priority above 69
    sched.sched_priority = 69;

    pthread_t thread;
    pthread_attr_t attr;
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(&attr, &sched) == 0);

    assert(pthread_create(&thread, &attr, cyclic_task, NULL) == 0);

    double last = 0;
    double max0 = 0;
    while(1)
    {
        double max1 = max_us;
        int n;
        printf("\033[2J");
        if(max1 != max0)
        {
            printf("max latency %f (us)\n", max_us);
        }
        for(n = 0; n < MAX_BINS; n++)
        {
            printf("%f us count %f\n", n * 1.0 / MAX_BINS * MAX_LATENCY, hist[n]);
        }

        max0 = max1;
        usleep(1000000);
    }
    
    pause();
    
    return 0;
}

/****************************************************************************/
