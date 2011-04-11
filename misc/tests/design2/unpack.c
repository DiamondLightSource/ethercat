#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "classes.h"
#include "parser.h"
#include "rtutils.h"

static EC_CONFIG * cfg;

int unpack_int(char * buffer, int * ofs)
{
    int value = *((int *)(buffer + *ofs));
    (*ofs) += sizeof(int);
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
    case 32:
        value = *(int32_t *)buffer;
        // no unsigned 32 bit type in EPICS
        break;
    default:
        printf("unknown type\n");
    }
    return value;        
}

int pdo_data(char * buffer, int size)
{
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
    NODE * node;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        NODE * node1;
        for(node1 = listFirst(&device->pdo_entry_mappings); node1; node1 = node1->next)
        {
            EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node1;

            printf(" %s ", mapping->pdo_entry->parent->name);
            printf( "%s ", mapping->pdo_entry->datatype);

            if(mapping->pdo_entry->oversampling)
            {
                for(n = 0; n < mapping->parent->oversampling_rate; n++)
                {
                    printf("%d ", cast_int32(mapping, buffer, n));
                }
                printf("\n");
            }
            else
            {
                int32_t val = cast_int32(mapping, buffer, 0);
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
    int scanner_config_size = unpack_int(buffer, &ofs);
    read_config2(buffer + ofs, scanner_config_size, cfg);
    ofs += scanner_config_size;
    int mapping_config_size = unpack_int(buffer, &ofs);
    parseEntriesFromBuffer(buffer + ofs, mapping_config_size, cfg);

    NODE * node;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        NODE * node1;
        for(node1 = listFirst(&device->pdo_entry_mappings); node1; node1 = node1->next)
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
