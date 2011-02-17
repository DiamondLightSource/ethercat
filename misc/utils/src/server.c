/* turn on unix98 features such as PTHREAD_PRIO_INHERIT */
#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

#include <ecrt.h>

#include "commands.h"
#include "slaves.h"

int ethercat_enabled = 1;

/* #define ENABLE_LOG_DEBUG */
#define ENABLE_LOG_WARNING
#include "log.h"

/* maximum number of supported PDOs */
#define MAX_REGS 1024
/* maximum number of clients */
#define MAX_CLIENTS 16
/* max virtual channels */
#define MAX_CHANNELS 256
#define END_CHANNEL 0xFF

/* realtime configuration */
#define CLOCK_TO_USE CLOCK_MONOTONIC
#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS    (1000000L)
#define BUFSIZE 1000

#define TICK_CMD 1

char * socket_name = "/tmp/scanner.sock";
#define BACKLOG 5

/* maps virtual channel number to offset and size in PDO */
struct channel_map_entry
{
    uint8_t channel;
    uint8_t assigned;    /* set to 0 for fill-in entries*/
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    unsigned int offset;
    unsigned int bit_position;
    uint16_t alias;
};
typedef struct channel_map_entry channel_map_entry;

/* timer object */

struct nanotimer_t
{
    struct timespec cycletime;
    pthread_t thread;
    pthread_attr_t attr;
    struct worker_t * worker;
    struct buffer_t * buffer;
};
typedef struct nanotimer_t nanotimer_t;

/* socket connection object */

struct connection_t
{
    int fd;
    int client;
    pthread_t reader_thread;
    pthread_t writer_thread;
};
typedef struct connection_t connection_t;

/* worker object */

struct worker_t
{
    int tick;
    pthread_t thread;
    pthread_attr_t attr;
    struct buffer_t * input_buffer;
};
typedef struct worker_t worker_t;

/* bounded buffer object */

struct buffer_t
{
    int capacity;
    int itemsize;
    int size;
    int head;
    int tail;
    char * data;
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr;
    pthread_cond_t notempty;
    pthread_cond_t notfull;
    int urgent;
};
typedef struct buffer_t buffer_t;

/* client manager object */

struct client_manager_t
{
    buffer_t ready;
    buffer_t done;
};
typedef struct client_manager_t client_manager_t;

/* prototypes */

int buffer_get(buffer_t * buffer, void * data);
void buffer_put(buffer_t * buffer, void * data);
int buffer_tryput(buffer_t * buffer, void * data);
void buffer_urgent(buffer_t * buffer, int msg);
void local_buffer_get(buffer_t * buffer, void * data);
void local_buffer_put(buffer_t * buffer, void * data);

void show_priority(pthread_t * thread, char * name);

/* global objects */
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
uint8_t *domain1_pd = NULL;
nanotimer_t timer = {{0, PERIOD_NS}};
worker_t worker = {0};
buffer_t buffer = {BUFSIZE, sizeof(cmd_scanner)};
int clients = 0;

/* EtherCAT bus configuration */
channel_map_entry channel_map[MAX_CHANNELS];
int channel_map_n_entries;

/* client queues */
buffer_t client_buffers[MAX_CLIENTS];
buffer_t debug_buffer;
pthread_t debug_thread;

/* queue manager */
client_manager_t client_manager;

void * debug_start(void * usr)
{
    struct timespec ts;
    while(1)
    {
        buffer_get(&debug_buffer, &ts);
        //printf("tick @ %u %u\n", (unsigned)ts.tv_sec, (unsigned)ts.tv_nsec);
    }
}

void buffer_init(buffer_t * buffer, size_t itemsize, size_t capacity)
{
    buffer->itemsize = itemsize;
    buffer->capacity = capacity;
    buffer->data = (char *)malloc(capacity * itemsize);
    /* enable priority inheritance */
    assert(pthread_mutexattr_init(&buffer->mutexattr) == 0);
    assert(pthread_mutexattr_setprotocol(&buffer->mutexattr, 
                                         PTHREAD_PRIO_INHERIT) == 0);
    assert(pthread_mutex_init(&buffer->mutex, &buffer->mutexattr) == 0);
    assert(pthread_cond_init(&buffer->notfull, NULL) == 0);
    assert(pthread_cond_init(&buffer->notempty, NULL) == 0);
}

void * cm_start(void * usr)
{
    client_manager_t * cm  = (client_manager_t *)usr;
    int n;
    // fill the ready queue with client ids
    for(n = 0; n < MAX_CLIENTS; n++)
    {
        buffer_put(&cm->ready, &n);
    }
    // as client ids are released
    // put them back on the ready queue
    while(1)
    {
        buffer_get(&cm->done, &n);
        buffer_put(&cm->ready, &n);
    }
}

void init_client_manager()
{
    pthread_t cm_thread;
    buffer_init(&client_manager.ready, sizeof(int), MAX_CLIENTS);
    buffer_init(&client_manager.done, sizeof(int), MAX_CLIENTS);
    assert(pthread_create(&cm_thread, NULL, 
                          cm_start, &client_manager) == 0);
}

ssize_t read_all(int fd, void *buf, size_t count)
{
    size_t remaining = count;
    char * ofs = buf;
    while(remaining != 0)
    {
        ssize_t nbytes = read(fd, ofs, remaining);
        /* retry in case of interrupted system call */
        if(nbytes == -1 && errno == EINTR)
        {
            LOG_DEBUG(("continuing\n"));
            continue;
        }
        if(nbytes == -1)
        {
            return -1;
        }
        if(nbytes == 0)
        {
            /* EOF */
            return 0;
        }
        ofs += nbytes;
        remaining -= nbytes;
    }
    return count;
}

void * reader_start(void * usr)
{
    connection_t * connection = (connection_t *)usr;

    int socket_fd = connection->fd;

    int nbytes;

    LOG_DEBUG(("connected\n"));
    
    cmd_packet packet = {0};
    cmd_scanner scancmd = {0};

    while(1)
    {
        channel_map_entry * entry = NULL;

        nbytes = read_all(socket_fd, &packet, sizeof(packet));
        if(nbytes == -1 || nbytes == 0)
        {
            LOG_DEBUG(("client exit\n"));
            break;
        }
        LOG_DEBUG(("channel %d\n", packet.channel));
        
        if(packet.channel < channel_map_n_entries)
        {
            entry = &channel_map[packet.channel];
        }

        if(entry != NULL && entry->channel != END_CHANNEL)
        {
            scancmd.cmd = packet.cmd;
            scancmd.channel = packet.channel;
            scancmd.offset = entry->offset;
            scancmd.value = packet.value;
            scancmd.bit_length = entry->bit_length;
            scancmd.client = connection->client;
            
            buffer_put(worker.input_buffer, &scancmd);
        }
        else
        {
            scancmd.cmd = NOP;
            buffer_put(&client_buffers[connection->client], &scancmd);
        }
    }
    
    LOG_DEBUG(("closing socket\n"));
    
    // close(socket_fd);
    LOG_DEBUG(("reader thread exit\n"));
    
    // kill writer thread
    scancmd.cmd = KILL;
    buffer_put(&client_buffers[connection->client], &scancmd);
    
    return 0;
}

void * writer_start(void * usr)
{
    connection_t * connection = (connection_t *)usr;
    cmd_scanner scancmd = {0};
    cmd_packet packet = {0};

    while(1)
    {
        buffer_get(&client_buffers[connection->client], &scancmd);

        if(scancmd.cmd == KILL)
        {
            /* this thread is done */
            break;
        }

        packet.cmd = scancmd.cmd;
        packet.channel = scancmd.channel;
        packet.value = scancmd.value;
        /* TODO need to loop until done in case of signal */
        ssize_t nwrite = send(connection->fd, &packet, 
                              sizeof(packet), MSG_NOSIGNAL);
        if(nwrite == -1)
        {
            perror("writer_start");
            break;
        }
    }
    LOG_DEBUG(("writer thread exit\n"));
    
    // clear monitors
    scancmd.cmd = DISCONN;
    scancmd.client = connection->client;
    buffer_put(worker.input_buffer, &scancmd);
    
    return 0;
}

int connection_handler(int socket_fd)
{

    LOG_DEBUG(("new connection\n"));
    connection_t * connection = calloc(1, sizeof(connection_t));
    connection->fd = socket_fd;
    // request a new client queue
    buffer_get(&client_manager.ready, &connection->client);
    assert(pthread_create(&connection->reader_thread, NULL, 
                          &reader_start, connection) == 0);
    assert(pthread_create(&connection->writer_thread, NULL, 
                          &writer_start, connection) == 0);
    return 0;
}

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

void * timer_start(void * usr)
{
    nanotimer_t * timer = (nanotimer_t *)usr;
    struct timespec wakeupTime;
    clock_gettime(CLOCK_TO_USE, &wakeupTime);

    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, timer->cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        /* priority timer message to work queue */
        buffer_urgent(timer->buffer, TICK_CMD);
    }
    return 0;
}

void queue_filter_clients(buffer_t * q, int client)
{
    cmd_scanner scancmd = {0};
    int size = q->size;
    while(size != 0)
    {
        size--;
        local_buffer_get(q, &scancmd);
        if(scancmd.client != client)
        {
            local_buffer_put(q, &scancmd);
        }
        else
        {
            LOG_DEBUG(("removing client %d command from queue\n", client));
        }
    }
}

void show_priority(pthread_t * thread, char * name)
{
    struct sched_param sched = {0};
    int policy = 0;
    assert(pthread_getschedparam(*thread, &policy, &sched) == 0);
    printf("%s: policy %d priority %d\n", 
           name, policy, sched.sched_priority);
}

void * worker_start(void * usr)
{
    /* local queue for commands */
    buffer_t command_queue = {BUFSIZE, sizeof(cmd_scanner)};
    command_queue.data = malloc(command_queue.capacity * command_queue.itemsize);

    worker_t * worker = (worker_t *)usr;
    cmd_scanner scancmd = {0};

    show_priority(&worker->thread, "worker");

    struct timespec wakeupTime;

    while(1)
    {
        
        int tick = buffer_get(worker->input_buffer, &scancmd);
        
        if(tick)
        {

            // ETHERCAT bus cycle

            if(ethercat_enabled)
            {
                ecrt_master_receive(master);
                ecrt_domain_process(domain1);
            }

            int size = command_queue.size;
            LOG_INFO(("command queue size %d\n", size));
            while(size != 0)
            {
                size--;
                local_buffer_get(&command_queue, &scancmd);

                switch(scancmd.cmd)
                {
                    
                case WRITE:
                    switch(scancmd.bit_length)
                    {
                    case 1:
                        EC_WRITE_U8(domain1_pd + scancmd.offset, scancmd.value);
                        break;
                    case 8:
                        EC_WRITE_U8(domain1_pd + scancmd.offset, scancmd.value);
                        break;
                    case 16:
                        EC_WRITE_U16(domain1_pd + scancmd.offset, scancmd.value);
                        break;
                    case 32:
                        EC_WRITE_U32(domain1_pd + scancmd.offset, scancmd.value);
                        break;
                    }
                    break;
                case READ:
                case MONITOR:
                    switch(scancmd.bit_length)
                    {
                    case 8:
                        scancmd.value = EC_READ_U8(domain1_pd + scancmd.offset);
                        break;
                    case 16:
                        scancmd.value = (uint16_t)EC_READ_U16(domain1_pd + scancmd.offset);
                        break;
                    case 32:
                        scancmd.value = EC_READ_U32(domain1_pd + scancmd.offset);
                        break;
                    }
                    break;

                }
                
                LOG_DEBUG(("[timer] %s: offset(%d), size(%d), value(%llu)\n", 
                           command_strings[scancmd.cmd], scancmd.offset,
                           scancmd.bit_length, scancmd.value));
                
                // non-blocking put to client queue
                // need to check priorities of messages
                buffer_tryput(&client_buffers[scancmd.client], &scancmd);
                
                // if monitor, push back onto work queue
                if(scancmd.cmd == MONITOR)
                {
                    local_buffer_put(&command_queue, &scancmd);
                }
            }

            if(ethercat_enabled)
            {
                ecrt_domain_queue(domain1);
                ecrt_master_send(master);
            }
            
            clock_gettime(CLOCK_TO_USE, &wakeupTime);
            buffer_tryput(&debug_buffer, &wakeupTime);


        }
        else
        {
            // add this command to the local command list
            LOG_DEBUG(("[enqueue] %s: offset(%d), size(%d), value(%llu)\n", 
                       command_strings[scancmd.cmd], scancmd.offset,
                       scancmd.bit_length, scancmd.value));

            switch(scancmd.cmd)
            {
            case DISCONN:
                LOG_DEBUG(("client disconnect %d\n", scancmd.client));
                queue_filter_clients(&command_queue, scancmd.client);
                // release this client
                buffer_put(&client_manager.done, &scancmd.client);
                // TODO any spurious messages in the queue?
                break;
            default:
                local_buffer_put(&command_queue, &scancmd);
                break;
            }
        }
    }

    return 0;
}

int buffer_get(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->size == 0 && buffer->urgent == 0)
    {
        pthread_cond_wait(&buffer->notempty, &buffer->mutex);
    }
    /* urgent message - TODO VxWorks style add to head of queue instead? */
    if(buffer->urgent == 1)
    {
        int msg = buffer->urgent;
        buffer->urgent = 0;
        pthread_mutex_unlock(&buffer->mutex);
        return msg;
    }
    memcpy(data, buffer->data + buffer->tail * buffer->itemsize,
           buffer->itemsize);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
    pthread_cond_broadcast(&buffer->notfull);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

void local_buffer_get(buffer_t * buffer, void * data)
{
    assert(buffer->size != 0);
    memcpy(data, buffer->data + buffer->tail * buffer->itemsize,
           buffer->itemsize);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
}

void buffer_urgent(buffer_t * buffer, int msg)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->urgent = msg;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
}

void buffer_put(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->size == buffer->capacity)
    {
        pthread_cond_wait(&buffer->notfull, &buffer->mutex);
    }
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
}

int buffer_tryput(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    if(buffer->size == buffer->capacity)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

void local_buffer_put(buffer_t * buffer, void * data)
{
    assert(buffer->size != buffer->capacity);
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
}

void init_realtime_tasks()
{

    timer.worker = &worker;
    worker.input_buffer = &buffer;
    timer.buffer = &buffer;

    buffer_init(&buffer, sizeof(cmd_scanner), BUFSIZE);
    buffer_init(&debug_buffer, sizeof(struct timespec), 100000);

    /* allocate client buffers */
    int n;
    for(n = 0; n < MAX_CLIENTS; n++)
    {
        buffer_init(&client_buffers[n], sizeof(cmd_scanner), BUFSIZE);
    }

    /* setup real-time thread priorities */
    assert(pthread_attr_init(&timer.attr) == 0);
    assert(pthread_attr_init(&worker.attr) == 0);
    struct sched_param sched = {0};
    // 1-99, 99 is max, above 49 could starve sockets?
    // according to SOEM sample code
    sched.sched_priority = 60;

    // aha need EXPLICIT_SCHED or the default is
    // INHERIT_SCHED from parent process

    assert(pthread_attr_setinheritsched(&timer.attr,
                                        PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(&timer.attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(&timer.attr, &sched) == 0);

    assert(pthread_attr_setinheritsched(&worker.attr,
                                        PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(&worker.attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(&worker.attr, &sched) == 0);
    
    assert(pthread_create(&timer.thread, &timer.attr, timer_start, &timer)
           == 0);
    assert(pthread_create(&worker.thread, &worker.attr, worker_start, &worker)
           == 0);
    assert(pthread_create(&debug_thread, NULL, debug_start, NULL)
           == 0);
}

void show_registration(ec_pdo_entry_reg_t *domain1_regs)
{
    int n;
    int entry = 0;
    /* copy from channel map */
    for(n = 0; n < MAX_CHANNELS; n++)
    {   
        if (channel_map[n].channel == END_CHANNEL)
        {   
            break;
        }
        if ( channel_map[n].assigned )
        {
        printf("entry %d ", entry);
        printf("alias=%d ",  domain1_regs[entry].alias);
        printf("position=%d ", domain1_regs[entry].position);
        printf("index=0x%x ", domain1_regs[entry].index);
        printf("subindex=%d ", domain1_regs[entry].subindex);
        printf("offset=%u ",  *domain1_regs[entry].offset);
        printf("bit_position=%u \n", *domain1_regs[entry].bit_position);
        entry++;
        }
    }
}

void ethercat_init()
{
    // TODO from config file
    int n;
    ec_pdo_entry_reg_t domain1_regs[MAX_CHANNELS] = {{0}};
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
        domain1_regs[entry].alias = channel_map[n].alias;
        domain1_regs[entry].position = channel_map[n].position;
        domain1_regs[entry].vendor_id = channel_map[n].vendor_id;
        domain1_regs[entry].product_code = channel_map[n].product_code;
        domain1_regs[entry].index = channel_map[n].index;
        domain1_regs[entry].subindex = channel_map[n].subindex;
        domain1_regs[entry].offset = &channel_map[n].offset;
        domain1_regs[entry].bit_position = &channel_map[n].bit_position;
        entry++;
        }
    }
    master = ecrt_request_master(0);
    assert(master);
    domain1 = ecrt_master_create_domain(master);
    assert(domain1);
    assert(ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs) == 0);
    assert(ecrt_master_activate(master) == 0);

    domain1_pd = ecrt_domain_data(domain1);
    assert(domain1_pd);
    show_registration(domain1_regs);
}

#define LINEREAD_BUFSIZE 200
#define CONFIG_FILE "channel-map.txt"
void read_configuration()
{
    FILE *conf;
    char linebuf[LINEREAD_BUFSIZE];
    char *s;
    int c;
    int linecount = 0;
    int channel;
    unsigned int position;
    unsigned int index;
    int alias;
    channel_map_entry *mapping = channel_map;
    conf = fopen(CONFIG_FILE, "r");
    while ( (s = fgets(linebuf, LINEREAD_BUFSIZE, conf)) != NULL )
    {
        linecount++;
        if ( s[0] == '\n' ) {
            continue;
        }
        if ( s[0] == '#' ) {
            /* assume comment */
            continue;
        }
        c = sscanf(s, " %d, %d, %d, %d, 0x%x,  0x%x,  0x%x, 0x%x, %d ",
              &channel,
              (int *) &mapping->assigned,
              &alias,
              &position,
              (unsigned int *) &mapping->vendor_id,
              (unsigned int *) &mapping->product_code,
              &index,
              (unsigned int *) &mapping->subindex,
              (int *) &mapping->bit_length);
        if (c != 9)
        {
            printf("Error reading configuration line %d\n", linecount);
            exit(2);
        }
        if ( channel > MAX_CHANNELS)
        {
            printf("Error reading configuration line %d\n", linecount);
            printf("channel number must be smaller than %d\n", MAX_CHANNELS);
        }
        mapping->channel = channel;
        mapping->position = position;
        mapping->index = index;
        mapping->alias = alias;
        mapping++;
    }
    printf("read configuration okay\n");
    mapping->channel = END_CHANNEL;
    fclose(conf);
}

int main(void)
{

    if(ethercat_enabled)
    {
        read_configuration();
        ethercat_init();
    }
    else
    {
        domain1_pd = malloc(1000000 * sizeof(char));
    }

    init_client_manager();
    init_realtime_tasks();

    // lock memory
    if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    {
        perror("warning: memory lock error, try sudo");
    }
    
    struct sockaddr_un address;
    int socket_fd, connection_fd;
    size_t address_length;

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
        LOG_INFO(("socket() failed\n"));
        return 1;
    } 

    unlink(socket_name);
    address.sun_family = AF_UNIX;
    address_length = sizeof(address.sun_family) +
        sprintf(address.sun_path, socket_name);

    umask(0);
    if(bind(socket_fd, (struct sockaddr *) &address, address_length) != 0)
    {
        LOG_INFO(("bind() failed\n"));
        return 1;
    }

    if(listen(socket_fd, BACKLOG) != 0)
    {
        LOG_INFO(("listen() failed\n"));
        return 1;
    }

    while((connection_fd = accept(socket_fd, 
                                  (struct sockaddr *) &address,
                                  &address_length)) > -1)
    {
        connection_handler(connection_fd);
    }

    close(socket_fd);
    unlink(socket_name);
    return 0;
}

/*
  
  TODO
  
  check queue threading race conditions
  single cmd type merge packet_cmd and scanner_cmd

  priority channel (append to head of queue with overwrite)
  non-blocking circular buffers

  monitor filters (on change or client connect)
  
  prevent redundant monitor commands

  channels for scanner status e.g. detection of disconnected slaves
*/

