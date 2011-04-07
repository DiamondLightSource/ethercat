#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "classes.h"

typedef struct CONTEXT CONTEXT;
struct CONTEXT
{
    int depth;
    EC_CONFIG * config;
    EC_DEVICE_TYPE * device_type;
    EC_PDO * pdo;
    EC_PDO_ENTRY * pdo_entry;
    EC_SYNC_MANAGER * sync_manager;
    EC_DEVICE * device;
};

int expectNode(xmlNode * node, char * name)
{
    if((node->type == XML_ELEMENT_NODE) &&
       (strcmp((char *)node->name, name) == 0))
    {
        return 1;
    }
    else
    {
        printf("expected %s got %s", node->name, name);
        return 0;
    }
}

typedef int (*PARSEFUNC)(xmlNode *, CONTEXT *);

int parseChildren(xmlNode * node, CONTEXT * ctx, char * name, PARSEFUNC parseFunc)
{
    xmlNode * c;
    for(c = node->children; c; c = c->next)
    {
        if(c->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        if(!(expectNode(c, name) && parseFunc(c, ctx)))
        {
            return 0;
        }
    }
    return 1;
}

int getHex(xmlNode * node, char * name, int * value, int required)
{
    char * text = (char *)xmlGetProp(node, (unsigned char *)name);
    if(text != NULL)
    {
        int x;
        int count = sscanf(text, "0x%x", &x);
        if(count == 1)
        {
            *value = x;
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if(required)
        {
            printf("can't find %s\n", name);
        }
        else
        {
            *value = 0;
            return 1;
        }
    }
    return 0;
}

int getInt(xmlNode * node, char * name, int * value, int required)
{
    char * text = (char *)xmlGetProp(node, (unsigned char *)name);
    if(text != NULL)
    {
        *value = atoi(text);
        return 1;
    }
    else
    {
        if(required)
        {
            printf("can't find %s\n", name);
        }
        else
        {
            *value = 0;
            return 1;
        }
    }
    return 0;
}

int getStr(xmlNode * node, char * name, char ** value)
{
    *value = (char *)xmlGetProp(node, (unsigned char *)name);
    return (*value != NULL);
}

int parsePdoEntry(xmlNode * node, CONTEXT * ctx)
{
    ctx->pdo_entry = calloc(1, sizeof(EC_PDO_ENTRY));
    return 
        getHex(node, "index", &ctx->pdo_entry->index, 1) &&
        getHex(node, "subindex", &ctx->pdo_entry->sub_index, 1) &&
        getInt(node, "bit_length", &ctx->pdo_entry->bits, 1) &&
        getStr(node, "name", &ctx->pdo_entry->name) &&
        getInt(node, "length", &ctx->pdo_entry->oversampling, 0) &&
        listAdd(&ctx->pdo->pdo_entries, &ctx->pdo_entry->node);
}

int parsePdo(xmlNode * node, CONTEXT * ctx)
{
    ctx->pdo = calloc(1, sizeof(EC_PDO));
    return 
        getHex(node, "index", &ctx->pdo->index, 1) &&
        parseChildren(node, ctx, "entry", parsePdoEntry) &&
        listAdd(&ctx->sync_manager->pdos, &ctx->pdo->node);
}
             
int parseSync(xmlNode * node, CONTEXT * ctx)
{
    ctx->sync_manager = calloc(1, sizeof(EC_SYNC_MANAGER));
    return
        getInt(node, "dir", &ctx->sync_manager->direction, 1) &&
        getInt(node, "watchdog", &ctx->sync_manager->watchdog, 1) &&
        parseChildren(node, ctx, "pdo", parsePdo) &&
        listAdd(&ctx->device_type->sync_managers, &ctx->sync_manager->node);
}

int parseDeviceType(xmlNode * node, CONTEXT * ctx)
{
    ctx->device_type = calloc(1, sizeof(EC_DEVICE_TYPE));
    return
        getStr(node, "name", &ctx->device_type->name) &&
        getHex(node, "vendor", &ctx->device_type->vendor_id, 1) &&
        getHex(node, "product", &ctx->device_type->product_id, 1) &&
        parseChildren(node, ctx, "sync", parseSync) &&
        listAdd(&ctx->config->device_types, &ctx->device_type->node);
}

int parseTypes(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "device", parseDeviceType);
}

// chain parser

int parseDevice(xmlNode * node, CONTEXT * ctx)
{
    ctx->device = calloc(1, sizeof(EC_DEVICE));
    return 
        getStr(node, "name", &ctx->device->name) &&
        getStr(node, "type_name", &ctx->device->type_name) &&
        getInt(node, "position", &ctx->device->position, 1) &&
        listAdd(&ctx->config->devices, &ctx->device->node);
}

int parseChain(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "device", parseDevice);
}

int dump(xmlNode * node, CONTEXT * ctx)
{
    printf("%s %d\n", node->name, node->type);
    xmlNode * c;
    for(c = node->children; c; c = c->next)
    {
        if(c->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        if(!dump(c, ctx))
        {
            return 0;
        }
    }
    return 1;
}

void show(CONTEXT * ctx)
{
    printf("device types %d\n", ctx->config->device_types.count);
    NODE * node;
    for(node = listFirst(&ctx->config->device_types); node; node = node->next)
    {
        EC_DEVICE_TYPE * device_type = 
            (EC_DEVICE_TYPE *)node;
        printf("sync managers %d\n", device_type->sync_managers.count);
    }
    printf("device instances %d\n", ctx->config->devices.count);
    for(node = listFirst(&ctx->config->devices); node; node = node->next)
    {
        EC_DEVICE * device = 
            (EC_DEVICE *)node;
        printf("name %s position %d\n", device->name, device->position);
    }
}

EC_CONFIG * read_config(char * config, char * chain)
{
    LIBXML_TEST_VERSION;
    xmlDoc * doc = xmlReadFile(config, NULL, 0);
    assert(doc);
    CONTEXT ctx;
    ctx.config = calloc(1, sizeof(EC_CONFIG));
    if(parseTypes(xmlDocGetRootElement(doc), &ctx))
    {
        //show(&ctx);
    }
    else
    {
        printf("parse error\n");
    }
    xmlFreeDoc(doc);

    doc = xmlReadFile(chain, NULL, 0);
    assert(doc);
    if(parseChain(xmlDocGetRootElement(doc), &ctx))
    {
        //show(&ctx);
    }
    else
    {
        printf("parse error\n");
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return ctx.config;
}

int main_TEST_PARSER(int argc, char ** argv)
{
    LIBXML_TEST_VERSION; 
    assert(argc > 2);
    read_config(argv[1], argv[2]);
    return 0;
}
