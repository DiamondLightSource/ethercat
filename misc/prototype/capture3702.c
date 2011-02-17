/* capture3702.c
  capture input to analogue input record in a buffer.
  Scan at 1000 microseconds 
    usage: capture3702 <channel> <scan-rate>
*/
/* turn on unix98 features such as PTHREAD_PRIO_INHERIT */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <assert.h>

#include <ecrt.h>

#include "config.h"

/* realtime configuration */
#define CLOCK_TO_USE CLOCK_MONOTONIC
#define NSEC_PER_SEC (1000000000L)

#define FREQUENCY 1000
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)



/* max virtual channels */
#define MAX_CHANNELS 256

#define DEF_OUTPUT_FILE "/tmp/output.dat"
#define MAX_PATH_LEN 180

#define NSEC_PER_SEC (1000000000L)
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)


// offsets for PDO entries
//static int off_dig_out;
static int off_ain[100];
static int off_count;

/* worker object */
struct worker_t
{
    int  channel_no; /* channel to scan */
    long no_samples; /* samples to take */
    long no_batches; /* batches used to  take */
    int oversampling_factor;
    void *data;      /* data buffer */
    void *counter;     /* cycle count */
    int elem_size;   /* bytes per element */
    char output_file[MAX_PATH_LEN];
    int update_rate; /* micro seconds*/
    pthread_t thread;
    pthread_attr_t attr;
    struct timespec cycletime;
};
typedef struct worker_t worker_t;

/* global objects */
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
uint8_t *domain1_pd = NULL;
worker_t worker;

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;
    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) 
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    }
    else
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }
    return result;
}


void * worker_start(void * usr)
{
    worker_t * worker = (worker_t *)usr;
    char *data = (char *) worker->data;
    char *counter = (char *) worker->counter;
    long count = 0;
    int n_samples;
    int s;
    struct timespec wakeupTime;
    struct timespec app_time;
    
    clock_gettime(CLOCK_TO_USE, &wakeupTime);

    assert( worker->elem_size == 2);
    worker->no_batches = 0;
    n_samples = worker->oversampling_factor;
    if (n_samples > worker->no_samples - count)
        n_samples = worker->no_samples - count;
    while( count < worker->no_samples)
    {
        wakeupTime = timespec_add(wakeupTime, worker->cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        /* ETHERCAT bus cycle */
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);

        /* READ */
        for (s=0; s < n_samples; s++)
        {
            *((uint16_t *)data) = (uint16_t)EC_READ_U16(domain1_pd + 
                                        off_ain[s]);
            data += worker->elem_size;
        }
        *((uint16_t *)counter) = (uint16_t)EC_READ_U16(domain1_pd + off_count);
        counter += 2;
        worker->no_batches++;

        clock_gettime(CLOCK_TO_USE, &app_time);
        ecrt_master_application_time(master, TIMESPEC2NS(app_time));

        /* sync every cycle  */
        ecrt_master_sync_reference_clock(master);
        ecrt_master_sync_slave_clocks(master);
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);

        count += n_samples;
        if (n_samples > worker->no_samples - count)
            n_samples = worker->no_samples - count;
    }

    return 0;
}

void write_data(worker_t *worker)
{
    long count;
    char *heading = "ETHERCAT CAPTURE FILE 3702-v1.3";
    char *tail = "END OF CAPTURE FILE 3702";
    char buffer[1000];
    int buflen;
    int datafile = open(worker->output_file,
                        O_WRONLY + O_CREAT + O_TRUNC, 
                        S_IROTH + S_IRGRP + S_IRUSR + S_IWUSR);
    if (datafile < 0) {
        printf("could not open output file");
        return;
    }
    printf("Writing to output file %s\n", worker->output_file);
    
    write(datafile, heading, strlen(heading));
    sprintf(buffer, "channel: %d\n"
        "no_samples: %ld\n"
        "no_batches: %ld\n"
        "oversampling: %d\n"
        "element_size: %d\n"
        "output file: %s\n",
        worker->channel_no, worker->no_samples, worker->no_batches, 
        worker->oversampling_factor,
        worker->elem_size, worker->output_file);
    buflen = strlen(buffer);
    assert( write(datafile, &buflen, sizeof(buflen) ) > 0);
    assert( write(datafile, buffer, buflen) > 0);
    assert( write(datafile, &worker->elem_size, sizeof(int) ) > 0);
    assert( write(datafile, &worker->no_samples, sizeof(long) ) > 0);
    assert( write(datafile, &worker->no_batches, sizeof(long) ) > 0);
    assert( write(datafile, &worker->oversampling_factor, sizeof(int) ) > 0);
    assert( write(datafile, &worker->update_rate, sizeof(int) ) > 0);

    count = write(datafile, worker->data, 
                    worker->elem_size * worker->no_samples);
    assert( count == worker->elem_size * worker->no_samples);
    count = write(datafile, worker->counter, 2 * worker->no_batches);
    assert( count == 2 * worker->no_batches);
    assert( write(datafile, tail, strlen(tail)) > 0);
    close(datafile);
    printf("written data to %s\n", worker->output_file);
}

void worker_init(worker_t *w)
{
    int result;
    w->elem_size = 2;
    assert( w->elem_size != 0);
    w->oversampling_factor = 100;
    /* memory for worker buffer */
    w->data = calloc(w->no_samples, w->elem_size );
    assert( w->data );
    w->counter = calloc(w->no_samples / w->oversampling_factor + 1, 2);
    assert( w->counter );
    result = pthread_create(&w->thread, &w->attr, worker_start, w);
    if (result != 0 )
    {
        switch (result) {
        case EAGAIN: printf("EAGAIN:"); break;
        case EINVAL: printf("EINVAL:");break;
        case EPERM: printf("EPERM:"); break;
        default: printf("Unexpected result %d:", result);
        }
        printf("%s\n", strerror(result));
        assert(0);
    }
}

void init_realtime_tasks()
{
    /* setup real-time thread priorities */
    assert(pthread_attr_init(&worker.attr) == 0);
    struct sched_param sched = {0};
    // 1-99, 99 is max, above 49 could starve sockets?
    // according to SOEM sample code
    sched.sched_priority = 60;

    // aha need EXPLICIT_SCHED or the default is
    // INHERIT_SCHED from parent process

    assert(pthread_attr_setinheritsched(&worker.attr,
                                        PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(&worker.attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(&worker.attr, &sched) == 0);
}

void ethercat_init()
{
    ec_slave_config_t *sc;
    
    master = ecrt_request_master(0);
    assert(master);
    domain1 = ecrt_master_create_domain(master);
    assert(domain1);

    sc = ecrt_master_slave_config(master, 0, 2, 0x2,  0xe763052);
    assert(sc);

    int s;
    for (s = 1; s < worker.oversampling_factor; s++)
    {
        assert( ecrt_slave_config_pdo_mapping_add(sc, 0x1a00, 
                0x6000 + s * 0x0010, 0x02, 16) == 0);
    }
    for (s = 0; s < worker.oversampling_factor; s++)
    {
        off_ain[s] = ecrt_slave_config_reg_pdo_entry(sc, 
                0x6000 + s * 0x0010, 2, domain1, NULL);
    }
    off_count = ecrt_slave_config_reg_pdo_entry(sc, 0x6800, 2, domain1, NULL);

    ecrt_slave_config_dc(sc, 0x0730, PERIOD_NS / worker.oversampling_factor, 0, 
        PERIOD_NS, 0);

    assert(ecrt_master_activate(master) == 0);

    domain1_pd = ecrt_domain_data(domain1);
    assert(domain1_pd);
}

void usage()
{
    printf("Usage: capture list\n");
    printf("       capture <channel> <scan-rate-in-microsec> <no-samples>\n");
    printf("       capture -o <output-file> <channel> <scan-rate-in-microsec> <no-samples>\n");
}

void showSignals()
{
    printf("Channel available:\n");
    printf("1\n");
}

void parse_args(int argc, char *argv[])
{
    char *endptr = NULL;

    if (argc == 2)
        if ( strcmp("list", argv[1]) == 0){
            
            showSignals();
            exit(0);
        }

    if (argc == 6) {
        if ( strcmp("-o", argv[1]) != 0 ) {
            usage();
            exit(2);
        }
        strncpy(worker.output_file, argv[2], MAX_PATH_LEN);
        argc -= 2;
        argv = &argv[2];
    }
    else {
        strncpy(worker.output_file, DEF_OUTPUT_FILE, MAX_PATH_LEN);
    }
    
    if (argc == 4) {
        errno = 0;
        worker.channel_no = strtol(argv[1], &endptr, 10);
        assert(errno == 0);
        assert(*endptr == '\0');
        errno = 0;
        worker.update_rate = strtol(argv[2], &endptr, 10);
        assert(errno == 0);
        assert(*endptr == '\0');
        /* For now, only capture at 1000 microseconds */
        assert(worker.update_rate == 1000);
        worker.cycletime.tv_sec = 0;
        worker.cycletime.tv_nsec = 1000 * worker.update_rate;

        worker.no_samples = strtol(argv[3], &endptr, 10);
        assert(errno == 0);
        assert(*endptr == '\0');
        assert( worker.no_samples > 0 );
        printf("channel to capture: %d\n", worker.channel_no);
        printf("update rate : %d microseconds \n", worker.update_rate);
        printf("update rate : %ld nanoseconds \n", worker.cycletime.tv_nsec);
        printf("no_samples : %ld\n", worker.no_samples);
        printf("output file: %s\n", worker.output_file);
        worker.oversampling_factor = 100;
    }
    else {
      usage();
      exit(2);
    }
}

void wait_for_worker()
{
    int retval;
    int result;
    retval = pthread_join(worker.thread, (void * ) &result);
    assert(retval == 0);
}
int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    ethercat_init();

    init_realtime_tasks();

    // lock memory
    if ( mlockall(MCL_CURRENT | MCL_FUTURE) != 0 )
    {
        perror("warning: memory lock error, try sudo");
    }
    
    worker_init(&worker);
    wait_for_worker();
    write_data(&worker);
    return 0;
}
