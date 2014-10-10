#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <ellLib.h>

#include "classes.h"
#include "parser.h"
#include "rtutils.h"
#include "msgsock.h"
#include "messages.h"

static EC_CONFIG * cfg;

static int count = 0;

int pdo_data(char * buffer, int size);
int init_unpack(char * buffer, int size);

static int receive_config_on_connect(ENGINE * engine, int sock)
{
    int size = rtSockReceive(sock, engine->receive_buffer, engine->max_message);
    if(size > 0)
    {
        init_unpack(engine->receive_buffer, size);
    }
    // zero is success code
    return !(size > 0);
}

static int ioc_send(ENGINE * server, int size)
{
    if(count % 100 == 0)
    {
        EC_MESSAGE * msg = (EC_MESSAGE *) server->receive_buffer;
        if (msg->tag == MSG_PDO)
            pdo_data(server->receive_buffer, size);
    }
    count++;
    return 0;
}

static int ioc_receive(ENGINE * server)
{
    usleep(1000000);
    *(int *)server->send_buffer = MSG_HEARTBEAT;
    return sizeof(int);
}

int unpack_int(char * buffer, int * ofs)
{
    int value = *((int *)(buffer + *ofs));
    (*ofs) += sizeof(int);
    return value;
}

double cast_double(EC_PDO_ENTRY_MAPPING * mapping, char * buffer, int index)
{
    double value = 0;
    int bytes = (mapping->pdo_entry->bits -1) / 8 + 1;
    buffer += mapping->offset + index * bytes;

    /* According to the EtherCAT standard floats can come in two sizes: 32 bits
     * and 64 bits.
     */
    switch (mapping->pdo_entry->bits)
    {
        case 32:
            value = (double)*(float *)buffer;
            break;
        case 64:
            value = *(double *)buffer;
            break;
        default:
            printf("unknown type\n");
    }
    return value;
}

int32_t cast_int32(EC_PDO_ENTRY_MAPPING * mapping, char * buffer, int index)
{
    int32_t value = 0;
    int is_signed = 1;
    if(mapping->pdo_entry->datatype[0] == 'U' || mapping->pdo_entry->datatype[0] == 'B')
    {
        is_signed = 0;
    }
    
    int bytes = (mapping->pdo_entry->bits - 1) / 8 + 1;
    buffer += mapping->offset + index * bytes;
    

    switch(mapping->pdo_entry->bits)
    {
    case 1:
        value = *(uint8_t *)buffer;
        value = ((value & (1 << mapping->bit_position)) != 0);
        break;
    case 8:
        if(is_signed)
        {
            value = *(int8_t *)buffer;
        }
        else
        {
            value = *(uint8_t *)buffer;
        }
        break;
    case 16:
        if(is_signed)
        {
            value = *(int16_t *)buffer;
        }
        else
        {
            value = *(uint16_t *)buffer;
        }
        break;
    case 24:
        value = *(int32_t *)buffer;
        value &= 0x00ffffff;
        if (value & 0x800000)
            value |= 0xff000000; 
        break;
    case 32:
        if(is_signed)
        {
            value = *(int32_t *)buffer;
        }
        else
        {
            value = *(int32_t *)buffer;
            // WARNING no unsigned types in ASYN
            // discard top bit
            value &= INT32_MAX;
        }
        if(mapping->shift > 0)
            value = value >> mapping->shift;
        break;
    default:
        if(mapping->pdo_entry->bits > 32)
        {
            printf("unknown type\n");
        }
        else
        {
            value = *(int32_t *)buffer;
            value >>= mapping->bit_position;
            value &= (1 << mapping->pdo_entry->bits)-1;
        }
    }
    return value;        
}

int32_t sdocast_int32(EC_SDO_ENTRY *sdoentry,SDO_READ_MESSAGE *msg)
{
    int32_t value = 0;
    switch(sdoentry->bits)
    {
    case 1:
    case 8:
        value = *(uint8_t *)(msg->value);
        break;
    case 16:
        value = *(uint16_t *)(msg->value);
        break;
    }
    return value;
}
int pdo_data(char * buffer, int size)
{
    EC_MESSAGE * msg = (EC_MESSAGE *)buffer;
    assert(msg->tag == MSG_PDO);
    int n;
    for(n = 0; n < size; n++)
    {
        printf("%02x ", (unsigned char)buffer[n]);
        if((n % 8) == 15)
        {
            printf("\n");
        }
    }
    printf("\n");
    printf("bys cycle %d working counter %d state %d\n", msg->pdo.cycle, msg->pdo.working_counter, 
           msg->pdo.wc_state);
    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        ELLNODE * node1;
        for(node1 = ellFirst(&device->pdo_entry_mappings); node1; node1 = ellNext(node1))
        {
            EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node1;

            printf(" %s ", mapping->pdo_entry->parent->name);
            printf( "%s ", mapping->pdo_entry->datatype);

            if(mapping->pdo_entry->oversampling)
            {
                for(n = 0; n < mapping->parent->oversampling_rate; n++)
                {
                    printf("%d ", cast_int32(mapping, msg->pdo.buffer, n));
                }
                printf("\n");
            }
            else
            {
                int32_t val = cast_int32(mapping, msg->pdo.buffer, 0);
                printf("%d\n", val);
            }

        }
    }
    printf("\n");

    // store these in the ASYN int32 attributes for this device

    return 0;
}

int init_unpack(char * buffer, int size)
{
    cfg = calloc(1, sizeof(EC_CONFIG));
    int ofs = 0;
    int tag = unpack_int(buffer, &ofs);
    assert(tag == MSG_CONFIG);
    int scanner_config_size = unpack_int(buffer, &ofs);
    read_config(buffer + ofs, scanner_config_size, cfg);
    ofs += scanner_config_size;
    int mapping_config_size = unpack_int(buffer, &ofs);
    parseEntriesFromBuffer(buffer + ofs, mapping_config_size, cfg);

    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        ELLNODE * node1;
        for(node1 = ellFirst(&device->pdo_entry_mappings); node1; node1 = ellNext(node1))
        {
            EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node1;
            if(strcmp(mapping->pdo_entry->name, mapping->pdo_entry->parent->name) == 0)
            {
                printf("  %s\n", mapping->pdo_entry->name);
            }
            else
            {
                printf("  %s %s\n", mapping->pdo_entry->parent->name, mapping->pdo_entry->name);
            }
        }
    }
    return 0;
}

void test_ioc_client(char * path, int max_message)
{
    ENGINE * ioc = new_engine(max_message);
    ioc->path = path;
    ioc->connect = client_connect;
    ioc->on_connect = receive_config_on_connect;
    ioc->send_message = ioc_send;
    ioc->receive_message = ioc_receive;
    engine_start(ioc);
}

/*
    // write test data
    int n = 0;
    message_write wr;
    while(1)
    {
        wr.tag = MSG_WRITE;
        wr.ofs = 2;
        short * val = (short *)wr.data;
        *val = n;
        wr.mask[0] = 0xff;
        wr.mask[1] = 0xff;
        usleep(1000);
        // rtMessageQueueSend(scanner->workq, &wr, sizeof(wr));
        n++;
    }
*/

