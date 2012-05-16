#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

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

int debug = 1;
int selftest = 1;
int simulation = 0;
// latency histogram for test only
int dumplatency = 0;

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

void cyclic_task(void * usr)
{
    SCANNER * scanner = (SCANNER *)usr;
    ec_master_t * master = scanner->master;
    ec_domain_t * domain = scanner->domain;
    int simulation = scanner->simulation;
    uint8_t * pd = scanner->pd;
    struct timespec wakeupTime;
    EC_MESSAGE * msg = (EC_MESSAGE *)calloc(1, scanner->max_message);
    int tick = 0;

    uint8_t * write_cache = calloc(scanner->pdo_size, sizeof(char));
    uint8_t * write_mask = calloc(scanner->pdo_size, sizeof(char));

    ec_domain_state_t domain_state;
    if (simulation) 
    {
        domain_state.working_counter = scanner->config->devices.count;
        domain_state.wc_state = EC_WC_COMPLETE;
    }

    int nslaves = 0;
    ELLNODE * node;
    for(node = ellFirst(&scanner->config->devices); node; node = ellNext(node))
    {
        nslaves++;
    }

    while(1)
    {
        rtMessageQueueReceive(scanner->workq, msg, scanner->max_message);

        if(msg->tag == MSG_WRITE)
        {
            int bit;
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
            if (!simulation)
            {
                ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));
                
                // do this how often?
                ecrt_master_sync_reference_clock(master);
                ecrt_master_sync_slave_clocks(master);
                
                // gets reply to LRW frame sent last cycle
                // from network card buffer
                ecrt_master_receive(master);
                ecrt_domain_process(domain);
    
                ecrt_domain_state(domain, &domain_state);
            }
            else
                simulate_input(scanner);
            
            // merge writes
            int n;
            for(n = 0; n < scanner->pdo_size; n++)
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
                if (!simulation)
                {
                    EC_DEVICE * device = (EC_DEVICE *)node;
                    ecrt_master_get_slave(master, device->position, &slave_info);
                    msg->pdo.buffer[ofs++] = slave_info.al_state;
                    msg->pdo.buffer[ofs++] = slave_info.error_flag;
                }
                else
                {
                    msg->pdo.buffer[ofs++] = EC_AL_STATE_OP;
                    msg->pdo.buffer[ofs++] = 0;
                }
            }

            // distribute PDO
            int client;
            for(client = 0; client < scanner->max_clients; client++)
            {
                rtMessageQueueSendNoWait(scanner->clients[client]->q, msg, msg_size);
            }
            
            if (!simulation)
            {
                ecrt_domain_queue(domain);
                // sends LRW frame to be received next cycle
                ecrt_master_send(master);
            }
            
            tick++;
        }

    }

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
        simulation_fill(pdo_entry_mapping->sim_signal);
    }
    return 0;
}

int device_initialize(SCANNER * scanner, EC_DEVICE * device)
{
    int simulation_data_size = 0;
    ec_slave_config_t * sc = NULL;
    if (! scanner->simulation)
    {
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
        ELLNODE * node1 = ellFirst(&sync_manager->pdos);
        for(; node1; node1 = ellNext(node1))
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            if(debug)
            {
                printf("PDO:          index %x\n", pdo->index);
            }
            ELLNODE * node2 = ellFirst(&pdo->pdo_entries);
            for(; node2; node2 = ellNext(node2))
            {
                int n;
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                if(debug)
                {
                    printf("PDO ENTRY:    name \"%s\" index "
                           "%x subindex %x bits %d\n", 
                            pdo_entry->name, pdo_entry->index, 
                            pdo_entry->sub_index, pdo_entry->bits);
                }
                /* 
                    scalar entries are added automatically, just 
                    add oversampling extensions
                */
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
            ecrt_slave_config_dc(sc, 
                device->device_type->oversampling_activate, 
                PERIOD_NS / device->oversampling_rate, 0, PERIOD_NS, 0);
        }
    }

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
        printf("DEVICE:       name \"%s\" type \"%s\" position %d\n", \
               device->name, device->type_name, device->position);
        device_initialize(scanner, device);
    }
    printf("PDO SIZE:     %d\n", scanner->pdo_size);
    return 0;
}

SCANNER * start_scanner(char * filename, int simulation)
{
    SCANNER * scanner = calloc(1, sizeof(SCANNER));
    scanner->config = calloc(1, sizeof(EC_CONFIG));
    scanner->config_buffer = load_config(filename);
    assert(scanner->config_buffer);
    read_config2(scanner->config_buffer, strlen(scanner->config_buffer), scanner->config);
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
    }
    ethercat_init(scanner);
    if (simulation)
        scanner->domain = (ec_domain_t *) calloc(1, sizeof(SIM_DOMAIN));
                                            
    return scanner;
}

void build_config_message(SCANNER * scanner)
{
    // send config to clients
    EC_CONFIG * cfg = scanner->config;
    char * sbuf = serialize_config(cfg);
    scanner->config_message = calloc(scanner->max_message, sizeof(char));
    scanner->config_message->tag = MSG_CONFIG;
    char * buffer = scanner->config_message->config.buffer;
    int msg_ofs = 0;
    pack_string(buffer, &msg_ofs, scanner->config_buffer);
    pack_string(buffer, &msg_ofs, sbuf);
    scanner->config_size = buffer + msg_ofs - (char *)scanner->config_message;
}

static int queue_send(ENGINE * server, int size)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtMessageQueueSend(client->scanner->workq, server->receive_buffer, size);
}

static int queue_receive(ENGINE * server)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtMessageQueueReceive(client->q, server->send_buffer, server->max_message);
}
static int send_config_on_connect(ENGINE * server, int sock)
{
    CLIENT * client = (CLIENT *)server->usr;
    return rtSockSend(sock, client->scanner->config_message, client->scanner->config_size);
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
