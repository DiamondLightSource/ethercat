#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ecrt.h>
#include <unistd.h>

#include "ethercat_device2.h"

typedef struct context context;
struct context
{
    ethercat_device * dev;
    ec_sync_info_t * sync;
    ec_pdo_info_t * pdo;
    ec_pdo_entry_info_t * entry;
};

void check_element_fail(xmlNode * node, char * name)
{
    if(strcmp((char*)node->name, name) == 0)
    {
        return;
    }
    printf("error: found element <%s> expected <%s>\n", node->name, name);
    while(node)
    {
        if(node->type == XML_ELEMENT_NODE)
        {
            printf("in <%s> at line %d\n", node->name, node->line);
        }
        node = node->parent;
    }
}

void report_fail(xmlNode * node, char * name, int ok)
{
    if(ok)
    {
        return;
    }
    printf("error: attribute \"%s\" not found\n", name);
    while(node)
    {
        if(node->type == XML_ELEMENT_NODE)
        {
            printf("in <%s> at line %d\n", node->name, node->line);
        }
        node = node->parent;
    }
    exit(1);
}

char * get_attr_text(xmlNode * node, char * name)
{
    xmlAttrPtr p = xmlHasProp(node, (unsigned char *)name);
    if(p && p->children && p->children->content)
    {
        return (char *)(p->children->content);
    }
    else
    {
        return NULL;
    }
}

int get_hex_attr(xmlNode * node, char * name, int fail, int * ok)
{
    *ok = 0;
    char * text = get_attr_text(node, name);
    if(text)
    {
        int x;
        int count = sscanf(text, "0x%08x", &x);
        if(count == 1)
        {
            *ok = 1;
            return x;
        }
    }
    if(fail)
    {
        report_fail(node, name, *ok);
    }
    return 0;
}

int get_int_attr(xmlNode * node, char * name, int fail, int * ok)
{
    *ok = 0;
    char * text = get_attr_text(node, name);
    if(text)
    {
        int x;
        int count = sscanf(text, "%d", &x);
        if(count == 1)
        {
            *ok = 1;
            return x;
        }
    }
    if(fail)
    {
        report_fail(node, name, *ok);
    }
    return 0;
}

char * get_char_attr(xmlNode * node, char * name, int fail, int * ok)
{
    *ok = 0;
    char * text = get_attr_text(node, name);
    if(text)
    {
        *ok = 1;
        return strdup(text);
    }
    if(fail)
    {
        report_fail(node, name, *ok);
    }
    return strdup("\0");
}

void add_entry_metadata(char * name, int length, context * ctx)
{
    pdo_entry_type * node = calloc(1, sizeof(pdo_entry_type));
    node->name = name;
    node->length = length;
    node->sync = ctx->sync;
    node->pdo = ctx->pdo;
    node->entry = ctx->entry;
    
    node->next = ctx->dev->regs;
    ctx->dev->regs = node;
    
}

void parse_entry(xmlNode * node, context * ctx)
{
    ec_pdo_entry_info_t * entry = ctx->entry;
    int ok;
    entry->index = get_hex_attr(node, "index", 1, &ok);
    entry->subindex = get_hex_attr(node, "subindex", 1, &ok);
    entry->bit_length = get_int_attr(node, "bit_length", 1, &ok); 
    char * name = get_char_attr(node, "name", 1, &ok);
    int length = get_int_attr(node, "length", 0, &ok);
    if(!ok)
    {
        length = 1;
    }
    add_entry_metadata(name, length, ctx);
}

void parse_pdo(xmlNode * node, context * ctx)
{
    ec_pdo_info_t * pdo = ctx->pdo;
    xmlNode * n;
    int ok;
    pdo->index = get_hex_attr(node, "index", 1, &ok);
    pdo->n_entries = get_int_attr(node, "n_entries", 1, &ok);
    pdo->entries = calloc(pdo->n_entries, sizeof(ec_pdo_entry_info_t));
    int j = 0;
    for(n = node->children; n; n = n->next)
    {
        if(n->type == XML_ELEMENT_NODE)
        {
            check_element_fail(n, "entry");
            assert(j < pdo->n_entries);
            ctx->entry = pdo->entries + j;
            parse_entry(n, ctx);
            j++;
        }
    }
}

void parse_sync(xmlNode * node, context * ctx)
{
    ec_sync_info_t * sync = ctx->sync;
    xmlNode * n;
    int ok;
    sync->n_pdos = get_int_attr(node, "n_pdos", 1, &ok);
    sync->watchdog_mode = get_int_attr(node, "watchdog", 1, &ok);
    sync->dir = get_int_attr(node, "dir", 1, &ok);

    sync->pdos = calloc(sync->n_pdos, sizeof(ec_pdo_info_t));

    int j = 0;
    for(n = node->children; n; n = n->next)
    {
        if(n->type == XML_ELEMENT_NODE)
        {
            check_element_fail(n, "pdo");
            assert(j < sync->n_pdos);
            ctx->pdo = sync->pdos + j;
            parse_pdo(n, ctx);
            j++;
        }
    }
}

ethercat_device * parse_device(xmlNode * node)
{
    xmlNode * n;
    ethercat_device * dev = calloc(1, sizeof(ethercat_device));
    int ok;
    dev->slave_info.vendor_id = get_hex_attr(node, "vendor", 1, &ok);
    dev->slave_info.product_code = get_hex_attr(node, "product", 1, &ok);
    dev->slave_info.revision_number = get_hex_attr(node, "revision", 1, &ok);
    dev->name = get_char_attr(node, "name", 1, &ok);
    // zero default is ok for dcactivate
    dev->dcactivate = get_hex_attr(node, "dcactivate", 0, &ok);

    context ctx;
    ctx.dev = dev;

    int j = 0;
    for(n = node->children; n; n = n->next)
    {
        if(n->type == XML_ELEMENT_NODE)
        {
            check_element_fail(n, "sync");
            assert(j < EC_MAX_SYNC_MANAGERS);
            ctx.sync = dev->syncs + j;
            parse_sync(n, &ctx);
            j++;
        }
    }

    return dev;
}

ethercat_device * parse_devices(xmlNode * node)
{
    ethercat_device * head = NULL;
    xmlNode * n;
    for (n = node; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            xmlNode * c;
            for(c = n->children; c; c = c->next)
            {
                if(c->type == XML_ELEMENT_NODE)
                {
                    ethercat_device * dev = parse_device(c);
                    dev->next = head;
                    head = dev;
                }
            }
        }
    }
    return head;
}

ethercat_device_config * parse_device_config(xmlNode * node)
{
    ethercat_device_config * conf = calloc(1, sizeof(ethercat_device_config));
    int ok;
    conf->name = get_char_attr(node, "name", 1, &ok);
    conf->vaddr = get_int_attr(node, "vaddr", 1, &ok);
    conf->pos = get_int_attr(node, "pos", 1, &ok);
    conf->usr = get_char_attr(node, "user", 0, &ok);
    return conf;
}

ethercat_device_config * parse_chain(xmlNode * node)
{
    ethercat_device_config * head = NULL;
    xmlNode * n;
    for (n = node; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            xmlNode * c;
            for(c = n->children; c; c = c->next)
            {
                if(c->type == XML_ELEMENT_NODE)
                {
                    check_element_fail(c, "device");
                    ethercat_device_config * conf = parse_device_config(c);
                    conf->next = head;
                    head = conf;
                }
            }
        }
    }
    return head;
}

ethercat_device * parse_device_file(char * filename)
{
    printf("parsing device configuration \"%s\"\n", filename);
    xmlDoc * doc = xmlReadFile(filename, NULL, 0);
    assert(doc);
    ethercat_device * dev = parse_devices(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);
    return dev;
}

ethercat_device_config * parse_chain_file(char * filename)
{
    printf("parsing chain configuration \"%s\"\n", filename);
    xmlDoc * doc = xmlReadFile(filename, NULL, 0);
    assert(doc);
    ethercat_device_config * conf = parse_chain(xmlDocGetRootElement(doc));
    xmlFreeDoc(doc);
    return conf;
}

int main_TEST(int argc, char **argv)
{
    LIBXML_TEST_VERSION; 
    
    ethercat_device * devs = parse_device_file("config.xml");
    ethercat_device * d;
    for(d = devs; d != NULL; d = d->next)
    {
        printf("found device %s\n", d->name);
        pdo_entry_type * r;
        for(r = d->regs; r != NULL; r = r->next)
        {
            printf("found parameter %s\n", r->name);
        }
        printf("\n");
    }
    
    ethercat_device_config * confs = parse_chain_file("chain.xml");
    ethercat_device_config * c;
    for(c = confs; c != NULL; c = c->next)
    {
        printf("found configuration for device %s\n", c->name);
    }
    
    xmlCleanupParser();
    return 0;
}
   
