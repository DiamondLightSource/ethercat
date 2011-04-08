#include <stdlib.h>
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

