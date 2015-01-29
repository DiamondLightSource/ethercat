#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ellLib.h>
#include <assert.h>


#include "classes.h"


EC_DEVICE * find_device(EC_CONFIG * cfg, int position)
{
    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;

        if(device->position == position)
        {
            return device;
        }
    }
    return NULL;
}

EC_PDO_ENTRY * find_pdo_entry(EC_DEVICE * device, int pdo_index, int index, int sub_index)
{
    ELLNODE * node0;
    for(node0 = ellFirst(&device->device_type->sync_managers); node0; node0 = ellNext(node0))
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        ELLNODE * node1;
        for(node1 = ellFirst(&sync_manager->pdos); node1; node1 = ellNext(node1))
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            if(pdo->index != pdo_index)
                continue;
            ELLNODE * node2;
            for(node2 = ellFirst(&pdo->pdo_entries); node2; node2 = ellNext(node2))
            {
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                if(pdo_entry->index == index && pdo_entry->sub_index == sub_index)
                {
                    return pdo_entry;
                }
            }
        }
    }
    return NULL;
}

EC_PDO_ENTRY * find_signal(EC_DEVICE * device, int signal_no, int bit_length)
{
    int count = 0;
    ELLNODE * node = ellFirst(&device->device_type->sync_managers);
    for ( ; node; node = ellNext(node))
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER * ) node;
        ELLNODE * node1 = ellFirst(&sync_manager->pdos);
        for ( ; node1; node1 = ellNext(node1) )
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            ELLNODE * node2 = ellFirst(&pdo->pdo_entries);
            for ( ; node2; node2 = ellNext(node2) )
            {
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *) node2;
                if (pdo_entry->bits != bit_length)
                    continue;
                count++;
                if (count == signal_no)
                    return pdo_entry;
            }
        }
    }
    return NULL;
}
EC_PDO_ENTRY_MAPPING * find_mapping(EC_DEVICE * device, int signal_no, 
                                        int bit_length)
{
    EC_PDO_ENTRY * pdo_entry = find_signal(device, signal_no, bit_length);
    if (!pdo_entry)
        return NULL;
    ELLNODE * node = ellFirst(&device->pdo_entry_mappings);
    for (; node; node = ellNext(node) )
    {
        EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = 
                    (EC_PDO_ENTRY_MAPPING *) node;
        if (pdo_entry_mapping->pdo_entry == pdo_entry)
        {
            return pdo_entry_mapping;

        }
    }
    return NULL;
}

EC_DEVICE_TYPE * find_device_type(EC_CONFIG * cfg, char * type_name, 
                                  int revision_id)
{
    ELLNODE * node;
    for(node = ellFirst(&cfg->device_types); node; node = ellNext(node))
    {
        EC_DEVICE_TYPE * device_type = (EC_DEVICE_TYPE *)node;
        if( (strcmp(device_type->name, type_name) == 0)
            && (device_type->revision_id == revision_id) )
        {
            return device_type;
        }
    }
    return NULL;
}

/* used in 99.9% of programs */
char * format(const char *fmt, ...)
{
    char * buffer = NULL;
    va_list args;
    va_start(args, fmt);
    int ret = vasprintf(&buffer, fmt, args);
    assert(ret != -1);
    va_end(args);
    return buffer;
}
