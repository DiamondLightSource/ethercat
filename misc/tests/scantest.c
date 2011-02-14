#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <malloc.h>
#include <assert.h>

#include "ecrt.h"
#include "rtutils.h"

rtMessageQueueId workq = NULL;

enum { MSG_TICK = 0 };

void timer_task(void * usr)
{
    struct timespec wakeupTime;
    struct timespec cycletime = {0, 1000000};
    int msg[1] = { MSG_TICK };
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        rtMessageQueueSendPriority(workq, msg, sizeof(msg));
    }
}

int oversampling_factor = 100;

ec_slave_config_t *sc;

/****************************************************************************/

// Application parameters
#define FREQUENCY 1000
#define CLOCK_TO_USE CLOCK_MONOTONIC
#define MEASURE_TIMING

/****************************************************************************/

#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC +       \
                       (B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)
  
/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

FILE * file_out = 0;

/****************************************************************************/

// process data
static uint8_t *domain1_pd = NULL;

#define BusCouplerPos    0, 0
#define DigOutSlavePos   0, 2
#define AoutSlavePos     0, 10

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2008 0x00000002, 0x07d83052
#define Beckhoff_EL2004 0x00000002, 0x07d43052
#define Beckhoff_EL4102 0x00000002, 0x10063052
#define Beckhoff_EL4732 0x00000002, 0x127c3052

#define Beckhoff_EL3702 0x00000002, 0x0e763052

#define EL4732Channel1  0x7000, 1

// offsets for PDO entries
//static int off_dig_out;
static int off_ain[100];
static int off_count;
//static int off_counter_in;
//static int off_counter_out;

static unsigned int counter = 0;
static unsigned int blink = 0;
static unsigned int sync_ref_counter = 0;
const struct timespec cycletime = {0, PERIOD_NS};

/*****************************************************************************/

void check_domain1_state(void)
{
    ec_domain_state_t ds;

    ecrt_domain_state(domain1, &ds);

    if (ds.working_counter != domain1_state.working_counter)
        printf("Domain1: WC %u.\n", ds.working_counter);
    if (ds.wc_state != domain1_state.wc_state)
        printf("Domain1: State %u.\n", ds.wc_state);

    domain1_state = ds;
}

/*****************************************************************************/

void check_master_state(void)
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding)
        printf("%u slave(s).\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printf("AL states: 0x%02X.\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printf("Link is %s.\n", ms.link_up ? "up" : "down");

    master_state = ms;
}

/****************************************************************************/
void dump_samples(FILE * f)
{
    int s;
    fprintf(f, "%d ", EC_READ_U16(domain1_pd + off_count));
    for(s = 0; s < oversampling_factor; s++)
    {
        fprintf(f, "%d ", EC_READ_S16(domain1_pd + off_ain[s]));
    }
    fprintf(f, "\n");
}


void cyclic_task(void * usr)
{
    int msg[4];

    int n = 0;
    struct timespec wakeupTime, time;
#ifdef MEASURE_TIMING
    struct timespec startTime, endTime, lastStartTime = {};
    uint32_t period_ns = 0, exec_ns = 0, latency_ns = 0,
        latency_min_ns = 0, latency_max_ns = 0,
        period_min_ns = 0, period_max_ns = 0,
        exec_min_ns = 0, exec_max_ns = 0;
#endif

    rtMessageQueueReceive(workq, msg, sizeof(msg));

    // get current time
    clock_gettime(CLOCK_TO_USE, &wakeupTime);

    while(1) 
    {

        n++;

        wakeupTime = timespec_add(wakeupTime, cycletime);

        rtMessageQueueReceive(workq, msg, sizeof(msg));

        // clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);

#ifdef MEASURE_TIMING
        clock_gettime(CLOCK_TO_USE, &startTime);
        latency_ns = DIFF_NS(wakeupTime, startTime);
        period_ns = DIFF_NS(lastStartTime, startTime);
        exec_ns = DIFF_NS(lastStartTime, endTime);
        lastStartTime = startTime;

        if (latency_ns > latency_max_ns) {
            latency_max_ns = latency_ns;
        }
        if (latency_ns < latency_min_ns) {
            latency_min_ns = latency_ns;
        }
        if (period_ns > period_max_ns) {
            period_max_ns = period_ns;
        }
        if (period_ns < period_min_ns) {
            period_min_ns = period_ns;
        }
        if (exec_ns > exec_max_ns) {
            exec_max_ns = exec_ns;
        }
        if (exec_ns < exec_min_ns) {
            exec_min_ns = exec_ns;
        }
#endif

        // receive process data
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);

        // check process data state (optional)
        check_domain1_state();

        if (counter) 
        {
            counter--;
        } 
        else 
        { // do this at 1 Hz
            counter = FREQUENCY;

            // check for master state (optional)
            check_master_state();

#ifdef MEASURE_TIMING
            // output timing stats
            printf("period     %10u ... %10u\n",
                   period_min_ns, period_max_ns); 
            printf("exec       %10u ... %10u\n",
                   exec_min_ns, exec_max_ns); 
            printf("latency    %10u ... %10u\n",
                   latency_min_ns, latency_max_ns); 
            period_max_ns = 0;
            period_min_ns = 0xffffffff;
            exec_max_ns = 0;
            exec_min_ns = 0xffffffff;
            latency_max_ns = 0;
            latency_min_ns = 0xffffffff;
#endif
        }

        // calculate new process data
        blink = !blink;

        // write process data
        // EC_WRITE_U16(domain1_pd + off_aout, blink ? 0x2000 : 0x0000);

        dump_samples(file_out);

        /*
        fprintf(file_out, "%d %d\n", 
                EC_READ_S16(domain1_pd + off_ain[0]), 
                EC_READ_U16(domain1_pd + off_count));
        */

        if((n % 100) == 0)
        {
            //dump_samples(stdout);
            /*
            printf("%d ", EC_READ_U16(domain1_pd + off_count));
            int s;
            for(s = 0; s < oversampling_factor; s++)
            {
                printf("%d ", EC_READ_S16(domain1_pd + off_ain[s]));
            }
            printf("\n");
            */
        }

        // write application time to master
        clock_gettime(CLOCK_TO_USE, &time);
        ecrt_master_application_time(master, TIMESPEC2NS(time));

        ecrt_master_sync_reference_clock(master);

        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; // sync every cycle
            ecrt_master_sync_reference_clock(master);
        }

        ecrt_master_sync_slave_clocks(master);
        // send process data
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);

#ifdef MEASURE_TIMING
        clock_gettime(CLOCK_TO_USE, &endTime);
#endif
    }
    fclose(file_out);
    printf("done\n");
}

/****************************************************************************/


int main(int argc, char **argv)
{

    file_out = fopen("dump.txt", "w");
    assert(file_out);

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        return -1;
    }
    
    master = ecrt_request_master(0);
    if (!master)
        return -1;

    domain1 = ecrt_master_create_domain(master);
    if (!domain1)
        return -1;

    sc = ecrt_master_slave_config(master, 0, 2, Beckhoff_EL3702);
    assert(sc);

    //assert(ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs) == 0);
    //ecrt_slave_config_pdo_assign_clear(sc, 0);
    //assert(ecrt_slave_config_pdo_assign_add(sc, 0, 0x1b00) == 0);
    //assert(ecrt_slave_config_pdo_assign_add(sc, 0, 0x1a00) == 0);
    // ecrt_slave_config_pdo_mapping_clear(sc, 0x1a00);

    int s;
    for(s = 1; s < oversampling_factor; s++)
    {
        assert(ecrt_slave_config_pdo_mapping_add(sc, 0x1a00, 0x6000 + s * 0x0010, 0x01, 16) == 0);
    }

    for(s = 0; s < oversampling_factor; s++)
    {
        off_ain[s] = ecrt_slave_config_reg_pdo_entry(sc, 0x6000 + s * 0x0010, 1, domain1, NULL);
    }

    off_count  = ecrt_slave_config_reg_pdo_entry(sc, 0x6800, 1, domain1, NULL);
    
    ecrt_slave_config_dc(sc, 0x0730, PERIOD_NS / oversampling_factor, 0, PERIOD_NS, 0);
    
    printf("Activating master...\n");
    if (ecrt_master_activate(master))
        return -1;

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }

    workq = rtMessageQueueCreate(10, 128);
    
    rtThreadId timer  = rtThreadCreate("timer",  60, 0, timer_task,  NULL);
    rtThreadId cyclic = rtThreadCreate("cyclic", 60, 0, cyclic_task, NULL);
    
    pause();
    
    return 0;
}

/* master sends status to subordinate threads at 1Hz ok */
