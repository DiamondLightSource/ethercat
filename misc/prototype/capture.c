/* capture.c
  capture input to analogue input record in a buffer.
  Scan at 50 microseconds 
    usage: capture <channel> <scan-rate>
    or capture <list> 
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
#define PERIOD_NS    (50000L)
#define BUFSIZE 1000

/* max virtual channels */
#define MAX_CHANNELS 256

#define DEF_OUTPUT_FILE "/tmp/output.dat"
#define MAX_PATH_LEN 180


/* worker object */
struct worker_t
{
    int  channel_no; /* channel to scan */
    channel_map_entry *map_entry;
    long no_samples; /* samples to take */
    void *data;      /* data buffer */
    struct timespec *time_data; /* debug data - timing info */
    struct timespec *wakeup_data; /* debug data - timing info */
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

/* EtherCAT bus configuration for EL4102 */
channel_map_entry channel_map[MAX_CHANNELS];
int channel_map_n_entries;

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
    channel_map_entry *e = worker->map_entry;
    char *data = (char *) worker->data;
    struct timespec *time_data = worker->time_data;
    struct timespec *wakeup_data = worker->wakeup_data;
    long count = 0;
    struct timespec wakeupTime;
    
    clock_gettime(CLOCK_TO_USE, &wakeupTime);

    while( count < worker->no_samples)
    {
        wakeupTime = timespec_add(wakeupTime, worker->cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        clock_gettime(CLOCK_TO_USE, wakeup_data);
        wakeup_data++;
        /* ETHERCAT bus cycle */
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);

        /* READ */
        switch(e->bit_length)
        {
        case 8:
            *data = EC_READ_U8(domain1_pd + e->offset);
            break;
        case 16:
            *((uint16_t *)data) = (uint16_t)EC_READ_U16(domain1_pd + e->offset);
            break;
        case 32:
            *((uint32_t *)data) = EC_READ_U32(domain1_pd + e->offset);
            break;
        } 
        data += worker->elem_size;
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);

        clock_gettime(CLOCK_TO_USE, time_data);
        time_data++;
        count++;
    }

    return 0;
}

void write_data(worker_t *worker)
{
    long count;
    char *heading = "ETHERCAT CAPTURE FILE v1.3";
    char *tail = "END OF CAPTURE FILE";
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
        "element_size: %d\n"
        "output file: %s\n",
        worker->channel_no, worker->no_samples,
        worker->elem_size, worker->output_file);
    buflen = strlen(buffer);
    assert( write(datafile, &buflen, sizeof(buflen) ) > 0);
    assert( write(datafile, buffer, buflen) > 0);
    assert( write(datafile, &worker->elem_size, sizeof(int) ) > 0);
    assert( write(datafile, &worker->no_samples, sizeof(long) ) > 0);
    assert( write(datafile, &worker->update_rate, sizeof(int) ) > 0);

    /* mapsize */
    buflen = sizeof(channel_map_entry);
    assert( write(datafile, &buflen, sizeof(buflen) ) > 0);
#define WRITE(elem,size) \
    assert( write(datafile, &worker->map_entry->elem, size) > 0);\
    assert( sizeof(worker->map_entry->elem) == size)
    WRITE(channel, 1);
    WRITE(assigned, 1); //2
    WRITE(position, 2); //4
    WRITE(vendor_id, 4);//8
    WRITE(product_code, 4);//12
    WRITE(index, 2);//14
    WRITE(subindex, 1);//15
    WRITE(bit_length, 1);//16
    WRITE(offset, 4);//20
    WRITE(bit_position, 4);//24
    WRITE(alias, 2);//26
    /* mapendcheck */
    assert( write(datafile, &buflen, sizeof(buflen) ) > 0);
    count = write(datafile, worker->data, 
                    worker->elem_size * worker->no_samples);
    assert( count == worker->elem_size * worker->no_samples);
    assert( write(datafile, tail, strlen(tail)) > 0);
    close(datafile);
    printf("written data to %s\n", worker->output_file);
}

void worker_init(worker_t *w)
{
    int result;
    long tspec_size = sizeof(struct timespec);
    w->elem_size = 0;
    switch (w->map_entry->bit_length) {
    case 8:
        w->elem_size = 1;
        break;
    case 16:
        w->elem_size = 2;
        break;
    case 32:
        w->elem_size = 4;
        break;
    }
    assert( w->elem_size != 0);
    /* memory for worker buffer */
    w->data = calloc(w->no_samples, w->elem_size );
    assert( w->data );
    w->time_data = (struct timespec *) calloc(w->no_samples, tspec_size);
    assert( w->time_data );
    w->wakeup_data = (struct timespec *) calloc(w->no_samples, tspec_size);
    assert( w->wakeup_data );
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
    int n;
    ec_pdo_entry_reg_t domain1_regs[MAX_CHANNELS] = {{0}};
    worker.map_entry = NULL;
    int entry = 0;
    /* copy from channel map */
    for(n = 0; n < MAX_CHANNELS; n++)
    {
        if (channel_map[n].channel == END_CHANNEL)
        {
            channel_map_n_entries = n;
            break;
        }
        if (! channel_map[n].assigned )
        {
            printf("skipping mapping %d\n", n);
        }
        else 
        {
        domain1_regs[entry].alias = 0;
        domain1_regs[entry].position = channel_map[n].position;
        domain1_regs[entry].vendor_id = channel_map[n].vendor_id;
        domain1_regs[entry].product_code = channel_map[n].product_code;
        domain1_regs[entry].index = channel_map[n].index;
        domain1_regs[entry].subindex = channel_map[n].subindex;
        domain1_regs[entry].offset = &channel_map[n].offset;
        domain1_regs[entry].bit_position = &channel_map[n].bit_position;
        entry++;
        if (channel_map[n].channel == worker.channel_no)
            worker.map_entry = &channel_map[n];
        }
    }
    assert( worker.map_entry != NULL );
    master = ecrt_request_master(0);
    assert(master);
    domain1 = ecrt_master_create_domain(master);
    assert(domain1);
    assert(ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs) == 0);
    show_registration(domain1_regs);
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
    int n;
    printf(
" Channel :: position, vendor_id, product_code, index, subindex, bit_length\n"); 
    for (n = 0; n < MAX_CHANNELS; n++)
    {
        if (channel_map[n].channel == END_CHANNEL)
            break;
        if (channel_map[n].assigned ) {
            printf(
            "%8d  :  %7d   0x%x   0x%x  0x%x  %3d  %d\n",
            channel_map[n].channel,
            channel_map[n].position, 
            channel_map[n].vendor_id,
            channel_map[n].product_code,
            channel_map[n].index,
            channel_map[n].subindex,
            channel_map[n].bit_length);
        }
    }
}

void parse_args(int argc, char *argv[])
{
    char *endptr = NULL;

    if (argc == 2)
        if ( strcmp("list", argv[1]) == 0){
            read_configuration(channel_map, MAX_CHANNELS);
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
    assert( read_configuration(channel_map, MAX_CHANNELS) == 0);
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


