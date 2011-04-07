#include "classes.h"

NODE * listFirst(LIST * list)
{
    return list->node.next;
}

int listAdd(LIST * list, NODE * node)
{
    node->next = list->node.next;
    list->node.next = node;
    list->count++;
    return 1;
}
