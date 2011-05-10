#include <stdlib.h>
#include <string.h>
#include "classes.h"

NODE * listFirst(LIST * list)
{
    return list->node.next;
}

int listAdd(LIST * pList, NODE * pNode)
{
  pNode->next = NULL;
  pNode->previous = pList->node.previous;

  if (pList->count)
    pList->node.previous->next = pNode;
  else
    pList->node.next = pNode;

  pList->node.previous = pNode;
  pList->count++;

  return 1;
}

EC_DEVICE * find_device(EC_CONFIG * cfg, int position)
{
    NODE * node;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        if(device->position == position)
        {
            return device;
        }
    }
    return NULL;
}

EC_PDO_ENTRY * find_pdo_entry(EC_DEVICE * device, int index, int sub_index)
{
    NODE * node0;
    for(node0 = listFirst(&device->device_type->sync_managers); node0; node0 = node0->next)
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        NODE * node1;
        for(node1 = listFirst(&sync_manager->pdos); node1; node1 = node1->next)
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            NODE * node2;
            for(node2 = listFirst(&pdo->pdo_entries); node2; node2 = node2->next)
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

EC_DEVICE_TYPE * find_device_type(EC_CONFIG * cfg, char * name)
{
    NODE * node;
    for(node = listFirst(&cfg->device_types); node; node = node->next)
    {
        EC_DEVICE_TYPE * device_type = (EC_DEVICE_TYPE *)node;
        if(strcmp(device_type->name, name) == 0)
        {
            return device_type;
        }
    }
    return NULL;
}

