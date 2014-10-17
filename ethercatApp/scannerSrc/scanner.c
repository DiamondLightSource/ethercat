
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include <ecrt.h>
#include <ellLib.h>
#include <iocsh.h>

#include "rtutils.h"
#include "msgsock.h"
#include "messages.h"

#include "classes.h"
#include "parser.h"
#include "unpack.h"
#include "simulation.h"

#include "liberror.h"

int debug = 1;
int selftest = 1;
int simulation = 0;
// latency histogram for test only
int dumplatency = 0;
// Time to wait for EtherCAT frame to return. Default to 50 us.
long frame_time_ns = 50000; 

enum { PERIOD_NS = 1000000 };
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

struct CLIENT;
typedef struct CLIENT CLIENT;

enum { HISTBUCKETS = 2000 };

typedef struct
{
    EC_MESSAGE * config_message;
    int config_size;
    EC_CONFIG * config;
    char * config_buffer;
    ec_master_t * master;
    ec_domain_t * domain;
    int pdo_size;
    uint8_t * pd;
    CLIENT ** clients;
    int max_message;
    int max_queue_message;
    int max_clients;
    int client_capacity;
    int work_capacity;
    rtMessageQueueId workq;
    int buckets[HISTBUCKETS];
    int max_latency;
    int sends;            /* flag to signal there are sdo reads to send */
    /* used in simulation*/
    int simulation;
    int sim_pdo_size;     /* bytes used after all current assignments */
    int sim_bits;         /* actual number of bits used, no alignments */
    int sim_aligned_bit;  /* next bit available after all current assignments */
} SCANNER;

typedef struct 
{
    uint8_t * process_data;
} SIM_DOMAIN;

struct CLIENT
{
    ENGINE * engine;
    rtMessageQueueId q;
    SCANNER * scanner;
    int id;
};

enum { PRIO_LOW = 0, PRIO_HIGH = 60 };

/* prototypes */
EC_DEVICE * get_device(SCANNER * scanner, int device_index);

void dump_latency_task(void * usr)
{
    SCANNER * scanner = (SCANNER *)usr;
    char filename[1024];
    int d = 0;
    while(1)
    {
        snprintf(filename, sizeof(filename)-1, "/tmp/scanlat%d.txt", d++);
        usleep(10000000);
        FILE * f = fopen(filename, "w");
        assert(f);
        fprintf(f, "-1 %d\n", scanner->max_latency);
        int n;
        for(n = 0; n < HISTBUCKETS; n++)
        {
            fprintf(f, "%d %d\n", n, scanner->buckets[n]);
        }
        fclose(f);
    }
}

void advance_sim_signal(st_signal *sim_signal)
{
    sim_signal->index++;
    if (sim_signal->index >= sim_signal->no_samples)
       sim_signal->index = 0;
}
void simulate_input(SCANNER * scanner)
{
    int n;
    ELLNODE * node = ellFirst(&scanner->config->devices);
    for( ; node ; node = ellNext(node) )
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        ELLNODE * node1 = ellFirst(&device->pdo_entry_mappings);
        for ( ; node1; node1 = ellNext(node1) )
        {
            EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = 
                                    ( EC_PDO_ENTRY_MAPPING *) node1;
            if (!pdo_entry_mapping->sim_signal)
                continue;
            st_signal *sim_signal = pdo_entry_mapping->sim_signal;
            assert (sim_signal->signalspec && sim_signal->perioddata);
            copy_sim_data(sim_signal, pdo_entry_mapping, scanner->pd);
            advance_sim_signal(sim_signal);
            assert(pdo_entry_mapping->pdo_entry);
            EC_PDO_ENTRY * pdo_entry = pdo_entry_mapping->pdo_entry;
            if(pdo_entry->oversampling)
            {
                for(n = 1; n < device->oversampling_rate; n++)
                {
                    copy_sim_data2(sim_signal, pdo_entry_mapping, scanner->pd, 
                                   n);
                    advance_sim_signal(sim_signal);
                }
            }
        }
    }
}

void read_sdo(EC_SDO_ENTRY *sdoentry)
{
    switch (sdoentry->bits)
    {
    case 8:
        sdoentry->sdodata.data8 = 
            EC_READ_U8(ecrt_sdo_request_data(sdoentry->sdo_request));
        /* printf("read_sdo 8-bit value = %d\n", sdoentry->sdodata.data8); */
        break;
    case 16:
        sdoentry->sdodata.data16 = 
            EC_READ_U16(ecrt_sdo_request_data(sdoentry->sdo_request));
        /* printf("read_sdo 16-bit value = %d\n", sdoentry->sdodata.data16); */
        break;
    }
    SDO_READ_MESSAGE *msg = (SDO_READ_MESSAGE *)sdoentry->readmsg;
    msg->value[0] = sdoentry->sdodata.data[0];
    msg->value[1] = sdoentry->sdodata.data[1];
    msg->value[2] = sdoentry->sdodata.data[2];
    msg->value[3] = sdoentry->sdodata.data[3];            
}

void write_sdo(EC_SDO_ENTRY *sdoentry)
{
    SDO_WRITE_MESSAGE *wmsg = (SDO_WRITE_MESSAGE *)sdoentry->writemsg;
    sdoentry->sdodata.data[0] = wmsg->value.cvalue[0];
    sdoentry->sdodata.data[1] = wmsg->value.cvalue[1];
    sdoentry->sdodata.data[2] = wmsg->value.cvalue[2];
    sdoentry->sdodata.data[3] = wmsg->value.cvalue[3];
    switch (sdoentry->bits)
    {
    case 8:
        EC_WRITE_U8(ecrt_sdo_request_data(sdoentry->sdo_request), 
                    sdoentry->sdodata.data8);
        ecrt_sdo_request_write(sdoentry->sdo_request);
        break;
    case 16:
        EC_WRITE_U16(ecrt_sdo_request_data(sdoentry->sdo_request), 
                     sdoentry->sdodata.data16);
        ecrt_sdo_request_write(sdoentry->sdo_request);
        break;
    }
    
}

void send_to_clients(SCANNER * scanner, void *data, unsigned int size)
{
    int client;
    for (client = 0; client < scanner->max_clients; client++)
    {
        rtMessageQueueSendNoWait(scanner->clients[client]->q,
                                 data, size);
    }
}

void process_sdo_read_request(SCANNER * scanner, SDO_REQ_MESSAGE *sdo_req)
{
    EC_DEVICE * device = get_device(scanner, sdo_req->device);
    assert( device != NULL);
    ELLNODE * node = ellFirst(&device->sdo_requests);
    for (; node; node = ellNext(node))
    {
        EC_SDO_ENTRY *sdoentry = (EC_SDO_ENTRY *) node;
        assert(sdoentry->parent->slave == device);
        if ((sdoentry->sdostate == SDO_PROC_IDLE) && 
            (sdoentry->parent->index == sdo_req->index) &&
            (sdoentry->subindex == sdo_req->subindex))
        {
            sdoentry->sdostate = SDO_PROC_REQ;
        }
    }
}


void copy_sdo_write(EC_SDO_ENTRY *sdoentry, SDO_WRITE_MESSAGE *sdo_write)
{
    SDO_WRITE_MESSAGE * wmsg = (SDO_WRITE_MESSAGE *) sdoentry->writemsg;
    assert(wmsg->device == sdo_write->device);
    assert(wmsg->index == sdo_write->index);
    assert(wmsg->subindex == sdo_write->subindex);
    assert(wmsg->bits == sdo_write->bits);
    wmsg->value.cvalue[0] = sdo_write->value.cvalue[0];
    wmsg->value.cvalue[1] = sdo_write->value.cvalue[1];
    wmsg->value.cvalue[2] = sdo_write->value.cvalue[2];
    wmsg->value.cvalue[3] = sdo_write->value.cvalue[3];
}

void process_sdo_write_request(SCANNER * scanner, SDO_WRITE_MESSAGE *sdo_write)
{
    EC_DEVICE * device = get_device(scanner, sdo_write->device);
    assert( device != NULL);
    ELLNODE * node = ellFirst(&device->sdo_requests);
    for (; node; node = ellNext(node))
    {
        EC_SDO_ENTRY *sdoentry = (EC_SDO_ENTRY *) node;
        assert(sdoentry->parent->slave == device);
        if ((sdoentry->sdostate == SDO_PROC_IDLE) && 
            (sdoentry->parent->index == sdo_write->index) &&
            (sdoentry->subindex == sdo_write->subindex))
        {
            sdoentry->sdostate = SDO_PROC_WRITEREQ;
            copy_sdo_write(sdoentry, sdo_write);
        }
    }
}

void cyclic_task(void * usr)
{
    SCANNER * scanner = (SCANNER *)usr;
    ec_master_t * master = scanner->master;
    ec_domain_t * domain = scanner->domain;

    int error_to_console = 1;
    int slave_info_status;
    uint8_t * pd = scanner->pd;
    struct timespec wakeupTime;
    EC_MESSAGE * msg = (EC_MESSAGE *)calloc(1, scanner->max_message);
    int tick = 0;

    uint8_t * write_cache = calloc(scanner->pdo_size, sizeof(char));
    uint8_t * write_mask = calloc(scanner->pdo_size, sizeof(char));

    ec_domain_state_t domain_state;

    int nslaves = 0;
    ELLNODE * node;
    for(node = ellFirst(&scanner->config->devices); node; node = ellNext(node))
    {
        nslaves++;
    }

    /* suppress errors to stderr */
    ecrt_err_to_stderr = 0;
    while(1)
    {
        rtMessageQueueReceive(scanner->workq, msg, scanner->max_message);

        if(msg->tag == MSG_WRITE)
        {
            int bit;
            if (msg->write.bits != 1 && msg->write.bit_position != 0)
            {
                printf("Assert will fail: "
                       "write bits %d, bit position %d\n",
                       msg->write.bits, msg->write.bit_position);
            }

            assert(msg->write.bits == 1 || msg->write.bit_position == 0);
            int value = msg->write.value;
            switch(msg->write.bits)
            {
            case 1:
                assert(msg->write.offset < scanner->pdo_size);
                bit = (1 << msg->write.bit_position);
                write_mask[msg->write.offset] |= bit;
                if(value)
                {
                    write_cache[msg->write.offset] |= bit;
                }
                else
                {
                    write_cache[msg->write.offset] &= ~bit;
                }
                break;
            case 8:
                assert(msg->write.offset < scanner->pdo_size);
                write_mask[msg->write.offset + 0] = 0xff;
                write_cache[msg->write.offset + 0] = value & 0xff;
                break;
            case 16:
                assert(msg->write.offset + 1 < scanner->pdo_size);
                write_mask[msg->write.offset + 0] = 0xff;
                write_mask[msg->write.offset + 1] = 0xff;
                write_cache[msg->write.offset + 0] = value & 0xff;
                write_cache[msg->write.offset + 1] = (value >> 8) & 0xff;
                break;
            case 32:
                assert(msg->write.offset + 3 < scanner->pdo_size);
                write_mask[msg->write.offset + 0] = 0xff;
                write_mask[msg->write.offset + 1] = 0xff;
                write_mask[msg->write.offset + 2] = 0xff;
                write_mask[msg->write.offset + 3] = 0xff;
                write_cache[msg->write.offset + 0] = value & 0xff;
                write_cache[msg->write.offset + 1] = (value >> 8) & 0xff;
                write_cache[msg->write.offset + 2] = (value >> 16) & 0xff;
                write_cache[msg->write.offset + 3] = (value >> 24) & 0xff;
                break;
            }
        }
        else if(msg->tag == MSG_HEARTBEAT)
        {
            // keeps connection alive
        }
        else if(msg->tag == MSG_TICK )
        {
            wakeupTime = msg->timer.ts;

            ecrt_master_receive(master);
            ecrt_domain_process(domain);
    
            ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));
            // do this how often?
            ecrt_master_sync_reference_clock(master);
            ecrt_master_sync_slave_clocks(master);
                
            ecrt_domain_state(domain, &domain_state);
            
            // Merge writes
            int n;
            for (n = 0; n < scanner->pdo_size; n++)
            {
                pd[n] &= ~write_mask[n];
                pd[n] |= write_cache[n];
            }
            memset(write_mask, 0, scanner->pdo_size);
            // check latency
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            msg->pdo.tv_sec = ts.tv_sec;
            msg->pdo.tv_nsec = ts.tv_nsec;
            struct timespec latency = timespec_sub(ts, wakeupTime);
            int lat_us = (int)(1e6 * (latency.tv_sec + latency.tv_nsec * 1e-9));
            if(lat_us < 0)
            {
                lat_us = 0;
            }
            // printf("Wakeup time %llu, latency: %d us\n", TIMESPEC2NS(wakeupTime), lat_us);
            
            if(lat_us > scanner->max_latency)
            {
                scanner->max_latency = lat_us;
            }
            
            if(lat_us > HISTBUCKETS - 1)
            {
                lat_us = HISTBUCKETS - 1;
            }
            
            if(dumplatency)
            {
                scanner->buckets[lat_us]++;
            }
            
            msg->pdo.working_counter = domain_state.working_counter;
            msg->pdo.wc_state = domain_state.wc_state;
            msg->pdo.tag = MSG_PDO;
            msg->pdo.cycle = tick;
            msg->pdo.size = scanner->pdo_size;
            memcpy(msg->pdo.buffer, pd, msg->pdo.size);
            int msg_size = (msg->pdo.buffer + msg->pdo.size + nslaves * 2 * sizeof(char)) - (char *)msg;

            /* master sends FPRD datagrams every cycle to get this slave info */
            ELLNODE * node;
            int ofs = msg->pdo.size;
            for(node = ellFirst(&scanner->config->devices); node; node = ellNext(node))
            {
                ec_slave_info_t slave_info;
                EC_DEVICE * device = (EC_DEVICE *)node;
                assert(device->position != -1);
                    
                slave_info_status = ecrt_master_get_slave(master, device->position, &slave_info);
                if (slave_info_status != 0)
                {
                    if (error_to_console)
                    {
                        fprintf(stderr,"etherlab library error: %s", ecrt_errstring);
                        error_to_console = 0;
                    }
                }
                else
                    error_to_console = 1;
                msg->pdo.buffer[ofs++] = slave_info.al_state;
                msg->pdo.buffer[ofs++] = slave_info.error_flag;
                if (device->sdo_requests.count > 0)
                {
                    ELLNODE * reqnode;
                    reqnode = ellFirst(&device->sdo_requests);
                    for (;reqnode; reqnode = ellNext(reqnode))
                    {
                        EC_SDO_ENTRY * devsdo = (EC_SDO_ENTRY *)reqnode;
                        switch (devsdo->sdostate)
                        {
                        case SDO_PROC_IDLE:
                            break;
                        case SDO_PROC_REQ:
                            devsdo->state = ecrt_sdo_request_state(devsdo->sdo_request);
                            if (devsdo->state != EC_REQUEST_BUSY)
                            {
                                devsdo->sdostate = SDO_PROC_READ;
                                ecrt_sdo_request_read(devsdo->sdo_request);
                            }
                            break;
                        case SDO_PROC_READ:
                            devsdo->state = ecrt_sdo_request_state(devsdo->sdo_request);
                            if (devsdo->state == EC_REQUEST_SUCCESS)
                            {
                                read_sdo(devsdo);
                                devsdo->sdostate = SDO_PROC_SEND;
                            }
                            else if (devsdo->state == EC_REQUEST_ERROR)
                            {
                                //TODO send error report to clients
                                printf("error reading sdo\n");
                                devsdo->sdostate = SDO_PROC_IDLE;
                            }
                            break;
                        case SDO_PROC_SEND:
                            devsdo->sdostate = SDO_PROC_IDLE;
                            send_to_clients(scanner, devsdo->readmsg,
                                            sizeof(SDO_READ_MESSAGE));
                            break;
                        case SDO_PROC_WRITEREQ:
                            devsdo->state = ecrt_sdo_request_state(devsdo->sdo_request);
                            if (devsdo->state != EC_REQUEST_BUSY)
                            {
                                write_sdo(devsdo);
                                devsdo->sdostate = SDO_PROC_WRITE;
                            }
                            break;
                        case SDO_PROC_WRITE:
                            devsdo->state = ecrt_sdo_request_state(devsdo->sdo_request);
                            if (devsdo->state == EC_REQUEST_SUCCESS)
                            {
                                devsdo->sdostate = SDO_PROC_IDLE;
                            }
                            else if (devsdo->state == EC_REQUEST_ERROR)
                            {
                                //TODO send error report to clients
                                printf("error writing sdo\n");
                                devsdo->sdostate = SDO_PROC_IDLE;
                            }
                            break;
                        }
                    }
                }
            }
            // distribute PDO
            send_to_clients(scanner, msg, msg_size);

            ecrt_domain_queue(domain);
            // sends LRW frame
            ecrt_master_send(master);

            tick++;
        } /* MSG_TICK */
        else if (msg->tag == MSG_SDO_REQ) 
        {
            process_sdo_read_request(scanner, &msg->sdo_req);
            SDO_REQ_MESSAGE *sdo_req = &msg->sdo_req;
            assert(sdo_req->tag == MSG_SDO_REQ);
            printf("process_sdo_read_request device %d index %x subindex %x bits %d \n",
                   sdo_req->device,
                   sdo_req->index, sdo_req->subindex, sdo_req->bits);
        }
        else if (msg->tag == MSG_SDO_WRITE)
        {
            process_sdo_write_request(scanner, &msg->sdo_write);
            SDO_WRITE_MESSAGE *sdo_w = &msg->sdo_write;
            assert(sdo_w->tag == MSG_SDO_WRITE);
            printf("process_sdo_write_request device %d index %x subindex %x bits %d \n",
                   sdo_w->device,
                   sdo_w->index, sdo_w->subindex, sdo_w->bits);
        }
    }
}


EC_DEVICE * get_device(SCANNER * scanner, int device_index)
{
    ELLNODE * node = ellFirst(&scanner->config->dcs_lookups);
    for (; node; node = ellNext(node))
    {
        EC_DCS_LOOKUP * dindex = (EC_DCS_LOOKUP *) node;
        if (dindex->position == device_index)
            return dindex->device;
    }
    return NULL;
}


// why is ecrt_domain_size not defined for userspace?
void adjust_pdo_size(SCANNER * scanner, int offset, int bits)
{
    int bytes = (bits-1) / 8 + 1;
    int top = offset + bytes;
    if(top > scanner->pdo_size)
    {
        scanner->pdo_size = top;
    }
}

/* simple allocation of at least one byte for every pdo entry */
int adjust_sim_pdo_size(SCANNER *scanner, int bits, unsigned int *bit_position)
{
    int bytes = (bits-1) / 8 + 1;
    scanner->sim_aligned_bit = scanner->sim_pdo_size * 8  + bits;
    scanner->sim_pdo_size += bytes;
    scanner->sim_bits += bits;
    /* always zero */
    *bit_position = 0;
    return (scanner->sim_aligned_bit - bits ) / 8;
}

int simulation_init(EC_DEVICE * device)
{
    printf("Simulation init for device at position %d\n", device->position);
    ELLNODE * node = ellFirst(&device->simspecs);
    for (; node ; node = ellNext(node) )
    {
        st_simspec *simspec = (st_simspec *) node;
        EC_PDO_ENTRY_MAPPING *pdo_entry_mapping = 
            find_mapping(device, simspec->signal_no, simspec->bit_length);
        assert( pdo_entry_mapping );
        assert( pdo_entry_mapping->sim_signal == NULL);
        pdo_entry_mapping->sim_signal = calloc(1, sizeof(st_signal));
        pdo_entry_mapping->sim_signal->signalspec = simspec;
        printf("pdo entry name %s signal_no %d bit_length %d \n",
               pdo_entry_mapping->pdo_entry->name, simspec->signal_no,
               simspec->bit_length);
        simulation_fill(pdo_entry_mapping->sim_signal);
    }
    return 0;
}

int size_in_bytes(int bits)
{
    int size = bits / 8 ;
    if ( (bits % 8) > 0 )
        size = size + 1;
    return size;
}

char * describe_request(EC_SDO_ENTRY * sdoentry)
{
    assert(sdoentry->desc == NULL);
    sdoentry->desc = 
        format("Name: %s, %x:%x, bits %d asyn param %s \"%s\"",
               sdoentry->parent->name, 
               sdoentry->parent->index,
               sdoentry->subindex,
               sdoentry->bits,
               sdoentry->asynparameter,
               sdoentry->description);
    return sdoentry->desc;
}


/* Called from device_initialize to add sdo requests as 
 registered in the device's configuration */
int add_sdo_requests(SCANNER * scanner, EC_DEVICE * device, 
                     ec_slave_config_t *sc)
{
    ELLNODE * node = ellFirst(&device->sdo_requests);
    for(; node; node = ellNext(node))
    {
        EC_SDO_ENTRY * e = (EC_SDO_ENTRY *) node;
        e->sdostate = SDO_PROC_IDLE;
        e->sdo_request = ecrt_slave_config_create_sdo_request(
            sc, e->parent->index, e->subindex, 
            size_in_bytes(e->bits));
        assert( e->sdo_request != NULL );
        printf("SDO request created: \n%s\n", describe_request(e));
        free(e->desc);
        e->readmsg = calloc(1, sizeof(SDO_READ_MESSAGE));
        e->writemsg = calloc(1, sizeof(SDO_WRITE_MESSAGE));
        SDO_READ_MESSAGE * msg = (SDO_READ_MESSAGE *) e->readmsg;
        assert( msg != NULL );
        msg->tag = MSG_SDO_READ;
        msg->device = device->position;
        msg->index = e->parent->index;
        msg->subindex = e->subindex;
        msg->bits = e->bits;
        msg->state = EC_REQUEST_UNUSED;
        SDO_WRITE_MESSAGE * wmsg = (SDO_WRITE_MESSAGE *) e->writemsg;
        assert( wmsg != NULL );
        wmsg->tag = MSG_SDO_WRITE;
        wmsg->device = device->position;
        wmsg->index = e->parent->index;
        wmsg->subindex = e->subindex;
        wmsg->bits = e->bits;
        wmsg->value.ivalue = 0;
    }
    return 0;
}

int device_initialize(SCANNER * scanner, EC_DEVICE * device)
{
    int simulation_data_size = 0;
    ec_slave_config_t * sc = NULL;
    if (! scanner->simulation)
    {
        assert(device->position != -1);
        sc = ecrt_master_slave_config(scanner->master, 0, device->position, 
            device->device_type->vendor_id, device->device_type->product_id);
        assert(sc);
    }

    ELLNODE * node0 = ellFirst(&device->device_type->sync_managers);
    for(; node0; node0 = ellNext(node0))
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        if(debug)
        {
            printf("SYNC MANAGER: dir %d watchdog %d\n", 
                sync_manager->direction, sync_manager->watchdog);
        }
        assert(ecrt_slave_config_sync_manager(sc,
                                              sync_manager->index,
                                              sync_manager->direction, 
                                              sync_manager->watchdog
                                              )==0);
        if (sync_manager->pdos.count > 0)
        {
            ecrt_slave_config_pdo_assign_clear(sc, sync_manager->index);
        }
        ELLNODE * node1 = ellFirst(&sync_manager->pdos);
        for(; node1; node1 = ellNext(node1))
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            if(debug)
            {
                printf("PDO:          index %x\n", pdo->index);
            }
            assert( ecrt_slave_config_pdo_assign_add(
                        sc, sync_manager->index, pdo->index) == 0);
            if (pdo->pdo_entries.count > 0)
            {
                ecrt_slave_config_pdo_mapping_clear(sc, pdo->index);
                
            }
            ELLNODE * node2 = ellFirst(&pdo->pdo_entries);
            for(; node2; node2 = ellNext(node2))
            {
                int n;
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                if(debug)
                {
                    printf("FIRST PASS PDO ENTRY:    name \"%s\" index "
                           "%x subindex %x bits %d\n", 
                            pdo_entry->name, pdo_entry->index, 
                            pdo_entry->sub_index, pdo_entry->bits);
                }
                assert( 
                    ecrt_slave_config_pdo_mapping_add(sc,
                                                      pdo->index, 
                                                      pdo_entry->index, 
                                                      pdo_entry->sub_index, 
                                                      pdo_entry->bits) == 0);
                if(pdo_entry->oversampling)
                {
                    for(n = 1; n < device->oversampling_rate; n++)
                    {
                        printf("mapping %x\n", pdo_entry->index + 
                                  n * pdo_entry->bits);
                        if (scanner->simulation)
                        {
                            simulation_data_size += pdo_entry->bits;
                        }
                        else
                        {
                            assert(ecrt_slave_config_pdo_mapping_add(
                                   sc, pdo->index, 
                                   pdo_entry->index + n * pdo_entry->bits,
                                   pdo_entry->sub_index, pdo_entry->bits) == 0);
                        } 
                    }
                }
                else
                    simulation_data_size += pdo_entry->bits;
            }
        }
    }
    if (debug)
    {
        printf("simulation bits count: %d\n", simulation_data_size);
    }
    
    /* second pass, assign mappings */
    node0 = ellFirst(&device->device_type->sync_managers);
    for(; node0; node0 = ellNext(node0))
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        ELLNODE * node1 = ellFirst(&sync_manager->pdos);
        for(; node1; node1 = ellNext(node1))
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            ELLNODE * node2 = ellFirst(&pdo->pdo_entries);
            for(; node2; node2 = ellNext(node2))
            {
                int n;
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = calloc(1, 
                                        sizeof(EC_PDO_ENTRY_MAPPING));
                if (!scanner->simulation)
                {
                     pdo_entry_mapping->offset = 
                        ecrt_slave_config_reg_pdo_entry(
                            sc, pdo_entry->index, pdo_entry->sub_index,
                            scanner->domain, 
                            (unsigned int *)&pdo_entry_mapping->bit_position);
                }
                else
                {
                    pdo_entry_mapping->offset = 
                        adjust_sim_pdo_size(scanner, pdo_entry->bits,
                         (unsigned int *)&pdo_entry_mapping->bit_position);
                }
                if(pdo_entry_mapping->offset < 0)
                {
                    fprintf(stderr, "Scanner: Failed to register "
                        "PDO entry %s on Device %s type %s at position %d\n", 
                            pdo_entry->name, device->name, device->type_name, 
                            device->position);
                    exit(1);
                }
                adjust_pdo_size(scanner, pdo_entry_mapping->offset, 
                                pdo_entry->bits);
                if(debug)
                {
                    printf("SECOND PASS PDO ENTRY:    name \"%s\" index "
                           "%x subindex %x bits %d MAPPING: offset %d bit_position %d\n", 
                           pdo_entry->name, pdo_entry->index, 
                           pdo_entry->sub_index, pdo_entry->bits, 
                           pdo_entry_mapping->offset, 
                           pdo_entry_mapping->bit_position);
                }

                if(pdo_entry->oversampling)
                {
                    for(n = 1; n < device->oversampling_rate; n++)
                    {
                        int bit_position;
                        int ofs;
                        if (!scanner->simulation)
                        {
                        ofs = ecrt_slave_config_reg_pdo_entry(
                            sc, pdo_entry->index + n * pdo_entry->bits, 
                            pdo_entry->sub_index, scanner->domain, 
                            (unsigned int *)&bit_position);
                        }
                        else
                        {
                            ofs = adjust_sim_pdo_size(scanner, pdo_entry->bits, 
                                            (unsigned int *)&bit_position);
                        } 
                        printf("pdo entry reg %08x %04x %d\n", 
                               pdo_entry->index + n * pdo_entry->bits, 
                               pdo_entry->sub_index, ofs);
                        assert(ofs == pdo_entry_mapping->offset + n * pdo_entry->bits / 8);
                        assert(bit_position == pdo_entry_mapping->bit_position);
                        adjust_pdo_size(scanner, ofs, pdo_entry->bits);
                    }
                }
                pdo_entry_mapping->pdo_entry = pdo_entry;
                pdo_entry_mapping->index = pdo_entry->index;
                pdo_entry_mapping->sub_index = pdo_entry->sub_index;
                ellAdd(&device->pdo_entry_mappings, &pdo_entry_mapping->node);
            }
        }
    }

    if(device->oversampling_rate != 0)
    {
        printf("oversamping rate %d\n", device->oversampling_rate);
        if (!scanner->simulation)
        {
            /* It would appear that to get a sane NextSync1Time value
               on the EL3702, we need to subtract the SYNC0 time from
               SYNC1. Putting a 10kHz signal into a 10kHz acquiring
               signal still gives some ugly sawtooth wave as the local
               clock syncs with the master clock, but its within
               100ns... */
            ecrt_slave_config_dc(sc, 
                device->device_type->oversampling_activate, 
                PERIOD_NS / device->oversampling_rate, 0,
                PERIOD_NS - (PERIOD_NS / device->oversampling_rate), 0);
        }
    }

    /* generate sdo request structures */
    add_sdo_requests(scanner, device, sc);
    if (scanner->simulation)
        simulation_init(device);
    return 0;
}

void pack_int(char * buffer, int * ofs, int value)
{
    buffer[*ofs + 0] = value & 0xff;
    buffer[*ofs + 1] = (value >> 8)  & 0xff;
    buffer[*ofs + 2] = (value >> 16) & 0xff;
    buffer[*ofs + 3] = (value >> 24) & 0xff;
    (*ofs) += 4;
}

void pack_string(char * buffer, int * ofs, char * str)
{
    int sl = strlen(str) + 1;
    pack_int(buffer, ofs, sl);
    strcpy(buffer + *ofs, str);
    (*ofs) += sl;
}

int ethercat_init(SCANNER * scanner)
{
    ELLNODE * node = ellFirst(&scanner->config->devices);
    for(; node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        assert(device->device_type);    
        printf("DEVICE:       name \"%s\" "
               "type \"%s\" rev %d position %d\n", 
               device->name, device->type_name, 
               device->type_revid, device->position);        
        device_initialize(scanner, device);
    }
    printf("PDO SIZE:     %d\n", scanner->pdo_size);
    return 0;
}

SCANNER * start_scanner(char * filename, int simulation)
{
    int n;
    SCANNER * scanner = calloc(1, sizeof(SCANNER));
    scanner->config = calloc(1, sizeof(EC_CONFIG));
    scanner->simulation = simulation;
    if (!simulation)
    {
        scanner->master = ecrt_request_master(0);
        if(scanner->master == NULL)
        {
            fprintf(stderr, "error: can't create "
                 "EtherCAT master - scanner already running?\n");
            exit(1);
        }
        scanner->domain = ecrt_master_create_domain(scanner->master);
        if(scanner->domain == NULL)
        {
            fprintf(stderr, "error: can't create EtherCAT domain\n");
            exit(1);
        }
        // get some info on each slave
        ec_master_info_t master_info;
        if(ecrt_master(scanner->master, &master_info) != 0) {
            fprintf(stderr, "can't get master info\n");
            exit(1);            
        }  
        ec_slave_info_t slave_info;
        for(n = 0; n < master_info.slave_count; n++) {        
            if( ecrt_master_get_slave(scanner->master, n, 
                                      &slave_info) != 0   ) 
            {
                fprintf(stderr, "can't get info for slave %d\n", n);
                exit(1);
            }
            EC_DCS_LOOKUP *dcs_lookup = calloc(1, sizeof(EC_DCS_LOOKUP));
            dcs_lookup->position = n;
            assert(slave_info.position == n);
            dcs_lookup->dcs = slave_info.serial_number;
            ellAdd(&scanner->config->dcs_lookups, &dcs_lookup->node);
        }
    }
    scanner->config_buffer = load_config(filename);
    assert(scanner->config_buffer);
    assert(PARSING_OKAY ==
           read_config(scanner->config_buffer, 
                       strlen(scanner->config_buffer), 
                       scanner->config));
    ethercat_init(scanner);
    if (simulation) 
    {
        scanner->domain = (ec_domain_t *) calloc(1, sizeof(SIM_DOMAIN));
    }
    return scanner;
}

void build_config_message(SCANNER * scanner)
{
    // send config to clients
    EC_CONFIG * cfg = scanner->config;
    scanner->config_message = calloc(scanner->max_message, sizeof(char));
    scanner->config_message->tag = MSG_CONFIG;
    char * buffer = scanner->config_message->config.buffer;
    int msg_ofs = 0;    
    char * xbuf = regenerate_chain(cfg);
    if (debug)
    {
        printf("Configuration string for EPICS\n%s\n", xbuf);
    }
    pack_string(buffer, &msg_ofs, xbuf);
    free(xbuf);
    char * sbuf = serialize_config(cfg);    
    if (debug)
    {
        printf("Mappings string for EPICS\n%s\n", sbuf);
    }
    pack_string(buffer, &msg_ofs, sbuf);
    free(sbuf);
    scanner->config_size = buffer + msg_ofs - 
                       (char *)scanner->config_message;
}

static int queue_send(ENGINE * server, int size)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtMessageQueueSend(client->scanner->workq, 
                              server->receive_buffer, size);
}

static int queue_receive(ENGINE * server)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtMessageQueueReceive(client->q, server->send_buffer, 
                                 server->max_message);
}
static int send_config_on_connect(ENGINE * server, int sock)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtSockSend(sock, client->scanner->config_message, 
                          client->scanner->config_size);
}

int main(int argc, char ** argv)
{
    int simulation = 0;
    opterr = 0;
    while (1)
    {
        int cmd = getopt (argc, argv, "qs");
        if(cmd == -1)
        {
            break;
        }
        switch(cmd)
        {
        case 'q':
            selftest = 0;
            break;
        case 's':
            simulation = 1;
            break;
        }
    }
    
    if(argc - optind < 2)
    {
        fprintf(stderr, "usage: scanner [-s] [-q] scanner.xml socket_path\n");
        exit(1);
    }
    
    char * xml_filename = argv[optind++];
    char * path = argv[optind++];

    fprintf(stderr, "Scanner xml(%s) socket(%s) PDO display(%d)\n", xml_filename, path, selftest);

    // start scanner
    SCANNER * scanner = start_scanner(xml_filename, simulation);
    scanner->max_message = 1000000;
    scanner->max_queue_message = 10000;
    scanner->max_clients = 10;
    scanner->client_capacity = 10;
    scanner->work_capacity = 10;

    build_config_message(scanner);

    // activate master
    if (!scanner->simulation)
    {
        ecrt_master_activate(scanner->master);
        scanner->pd = ecrt_domain_data(scanner->domain);
    }
    else
    {
        assert(scanner->domain);
        scanner->pd = (uint8_t *) calloc(1,scanner->sim_pdo_size);
    }
    if(scanner->pd == NULL)
    {
        fprintf(stderr, "%s %d: Scanner can't get domain data - check your "
            "configuration matches the devices on the bus.\n", __FILE__, __LINE__);
        exit(1);
    }

    scanner->workq = rtMessageQueueCreate(scanner->work_capacity, scanner->max_queue_message);
    scanner->clients = calloc(scanner->max_clients, sizeof(CLIENT *));

    printf("socket path is %s\n", path);
    int sock = rtServerSockCreate(path);
    if(sock == 0)
    {
        fprintf(stderr, "%s %d: Scanner can't create the UNIX socket - delete old sockets.\n", __FILE__, __LINE__);
        exit(1);
    }
    
    int c;
    for(c = 0; c < scanner->max_clients; c++)
    {
        CLIENT * client = calloc(1, sizeof(CLIENT));
        client->q = rtMessageQueueCreate(scanner->client_capacity, scanner->max_queue_message);
        client->scanner = scanner;
        scanner->clients[c] = client;
        // setup socket engine server side
        client->engine = new_engine(scanner->max_message);
        client->engine->listening = sock;
        client->engine->connect = server_connect;
        client->engine->on_connect = send_config_on_connect;
        client->engine->receive_message = queue_receive;
        client->engine->send_message = queue_send;
        client->engine->usr = client;
        engine_start(client->engine);
    }
    
    int prio = PRIO_HIGH;
    if(rtThreadCreate("cyclic", prio, 0, cyclic_task, scanner) == NULL)
    {
        printf("can't create high priority thread, fallback to low priority\n");
        prio = PRIO_LOW;
        assert(rtThreadCreate("cyclic", prio, 0, cyclic_task, scanner) != NULL);
    }
    new_timer(PERIOD_NS, scanner->workq, prio, MSG_TICK);

    if(dumplatency)
    {
        rtThreadCreate("dump", PRIO_LOW, 0, dump_latency_task, scanner);
    }

    if(selftest)
    {
        test_ioc_client(path, scanner->max_message);
    }

    iocsh(NULL);
    
    return 0;
}
