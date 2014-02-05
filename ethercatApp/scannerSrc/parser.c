#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <ellLib.h>
#include "classes.h"

int ellAddOK(ELLLIST * pList, ELLNODE * pNode)
{
    ellAdd(pList, pNode);
    return 1;
}

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
    EC_PDO_ENTRY_MAPPING * pdo_entry_mapping;
    st_simspec * simspec;
};

void show(CONTEXT * ctx)
{
    printf("== show == \n");
    printf("device types %d\n", ctx->config->device_types.count);
    ELLNODE * node;
    for(node = ellFirst(&ctx->config->device_types); node; node = ellNext(node))
    {
        EC_DEVICE_TYPE * device_type = 
          (EC_DEVICE_TYPE *)node;
        printf("name %s, ", device_type->name);
        printf(" (vendor %x, product %x, revision %d) ",
               device_type->vendor_id, device_type->product_id,
               device_type->revision_id);
        printf("sync managers %d\n", device_type->sync_managers.count);
    }
    printf("device instances %d\n", ctx->config->devices.count);
    for(node = ellFirst(&ctx->config->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("name %s position %d\n", device->name, device->position);
        printf("simulation specs %d\n", device->simspecs.count);
        ELLNODE * node1 = ellFirst(&device->simspecs);
        for (;node1; node1 = ellNext(node1) )
        {
            st_simspec * simspec = (st_simspec *) node1;
            printf("simspec signal_no %d type %d bit_length %d\n", 
                  simspec->signal_no, simspec->type, simspec->bit_length);
        }
    
    }
}

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

// octal constants are an absolute disaster, check them here
int isOctal(char * attr)
{
    enum { START, LEADING_ZERO } state = START;
    int n;
    for(n = 0; n < strlen(attr); n++)
    {
        if(isspace(attr[n]) || attr[n] == '+' || attr[n] == '-')
        {
            continue;
        }
        else if(state == START && attr[n] == '0')
        {
            state = LEADING_ZERO;
        }
        else if(state == LEADING_ZERO && isdigit(attr[n]))
        {
            return 1;
        }
        else
        {
            break;
        }
    }
    return 0;
}

int getDouble(xmlNode * node, char * name, double * value)
{
    char * text = (char *) xmlGetProp(node, (unsigned char *)name);
    char * end_str;
    if (text != NULL)
    {
        *value = strtod(text, &end_str);
        if ( *end_str == 0 )
            return 1;
    }
    printf("getDouble: Could not find parameter %s\n", name);
    return 0;
}

int getInt(xmlNode * node, char * name, int * value, int required)
{
    char * text = (char *)xmlGetProp(node, (unsigned char *)name);
    if(text != NULL)
    {
        if(isOctal(text))
        {
            printf("error: constants with leading zeros are disallowed as they are parsed as octal\n");
            return 0;
        }
        errno = 0;
        *value = strtol(text, NULL, 0);
        return !errno;
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
        getInt(node, "index", &ctx->pdo_entry->index, 1) &&
        getInt(node, "subindex", &ctx->pdo_entry->sub_index, 1) &&
        getInt(node, "bit_length", &ctx->pdo_entry->bits, 1) &&
        getStr(node, "name", &ctx->pdo_entry->name) &&
        getStr(node, "datatype", &ctx->pdo_entry->datatype) &&
        getInt(node, "oversample", &ctx->pdo_entry->oversampling, 1) &&
        (ctx->pdo_entry->parent = ctx->pdo) && 
        ellAddOK(&ctx->pdo->pdo_entries, &ctx->pdo_entry->node);
}

int parsePdo(xmlNode * node, CONTEXT * ctx)
{
    ctx->pdo = calloc(1, sizeof(EC_PDO));
    return 
        getStr(node, "name", &ctx->pdo->name) &&
        getInt(node, "index", &ctx->pdo->index, 1) &&
        parseChildren(node, ctx, "entry", parsePdoEntry) &&
        (ctx->pdo->parent = ctx->sync_manager) && 
        ellAddOK(&ctx->sync_manager->pdos, &ctx->pdo->node);
}
             
int parseSync(xmlNode * node, CONTEXT * ctx)
{
    ctx->sync_manager = calloc(1, sizeof(EC_SYNC_MANAGER));
    EC_SYNC_MANAGER *sm = ctx->sync_manager;
    return
        getInt(node, "index", &sm->index, 1) &&
        getInt(node, "dir", &sm->direction, 1) &&
        getInt(node, "watchdog", &sm->watchdog, 1) &&
        parseChildren(node, ctx, "pdo", parsePdo) &&
        (sm->parent = ctx->device_type) && 
        ellAddOK(&ctx->device_type->sync_managers, &sm->node);
}

int parseDeviceType(xmlNode * node, CONTEXT * ctx)
{
    ctx->device_type = calloc(1, sizeof(EC_DEVICE_TYPE));
    EC_DEVICE_TYPE *dt = ctx->device_type;
    return
        getStr(node, "name", &dt->name) &&
        getInt(node, "dcactivate", &dt->oversampling_activate, 0) &&
        getInt(node, "vendor", &dt->vendor_id, 1) &&
        getInt(node, "product", &dt->product_id, 1) &&
        getInt(node, "revision", &dt->revision_id, 1) &&
        parseChildren(node, ctx, "sync", parseSync) &&
        ellAddOK(&ctx->config->device_types, &dt->node);
}

int parseTypes(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "device", parseDeviceType);
}

/* chain parser */

int joinDevice(EC_CONFIG * cfg, EC_DEVICE * dv)
{
    dv->device_type = find_device_type(cfg, dv->type_name, 
                                       dv->type_revid);
    return (dv->device_type != NULL);
}


enum st_type parseStType(char * type_str)
{
    char *parseStrings[4] = {
     "constant", 
     "square_wave", 
     "sine_wave", 
     "ramp" 
    };
    int i = (int) ST_CONSTANT;
    while ( i < (int) ST_INVALID )
    {
        if ( strcmp(parseStrings[i], type_str) == 0 )
            return (enum st_type) i;
        i++;
    }
    return ST_INVALID;
}

int parseParams(xmlNode * node, st_simspec * spec)
{
    switch ( spec->type )
    {
        case ST_CONSTANT: 
            return getDouble(node, "value", &spec->params.pconst.value);
        case ST_SINEWAVE: 
            return getDouble(node, "low_value", &spec->params.psine.low) &&
                getDouble(node, "high_value", &spec->params.psine.high) &&
                getDouble(node, "period_ms", &spec->params.psine.period_ms);
        case ST_SQUAREWAVE:
            return getDouble(node, "low_value", &spec->params.psquare.low) &&
                getDouble(node, "high_value", &spec->params.psquare.high) &&
                getDouble(node, "period_ms", &spec->params.psquare.period_ms);
        case ST_RAMP:
            return getDouble(node, "low_value", &spec->params.pramp.low) &&
                getDouble(node, "high_value", &spec->params.pramp.high) &&
                getDouble(node, "period_ms", &spec->params.pramp.period_ms) &&
                getDouble(node, "symmetry", &spec->params.pramp.symmetry);
        case ST_INVALID:
        default:
            return 0;
    }
    return 0;
}

int find_duplicate(CONTEXT * ctx, int signal_no, int bit_length)
{
    ELLNODE * node = ellFirst(&ctx->device->simspecs);
    for ( ; node; node = ellNext(node) )
    {
        st_simspec * simspec = (st_simspec *)node;
        if ( (simspec->signal_no == signal_no) && (simspec->bit_length == bit_length) )
        {
            return 1;
        }
    }
    return 0;
}

int parseSimulation(xmlNode * node, CONTEXT * ctx)
{
    ctx->simspec = calloc(1, sizeof(st_simspec));
    char *type_str;
    if ( getStr(node, "signal_type", &type_str) 
            && getInt(node, "signal_no", &ctx->simspec->signal_no, 1) 
            && getInt(node, "bit_length", &ctx->simspec->bit_length, 1) )
    {
        if ( find_duplicate(ctx, ctx->simspec->signal_no, ctx->simspec->bit_length) )
        {
            printf("Duplicate signal number %d (bit_length %d) for device %s (position %d)\n", 
                ctx->simspec->signal_no, ctx->simspec->bit_length,
                ctx->device->name, ctx->device->position);
            assert(0);
        }
        ctx->simspec->type = parseStType(type_str);
        return parseParams(node, ctx->simspec)
                && ( ctx->simspec->parent = ctx->device )
                && ellAddOK(&ctx->device->simspecs, &ctx->simspec->node);
    }
    else
        return 0;
}

int parseSimspec(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "simulation", parseSimulation);
}

int parseDevice(xmlNode * node, CONTEXT * ctx)
{
    char * position_str;
    int dev_name_okay;
    int dev_revid_okay;
    int dev_type_name_okay;
    int joinOkay;
    ctx->device = calloc(1, sizeof(EC_DEVICE));
    EC_DEVICE * ctdev = ctx->device;
    ctdev->position = -1;
    getStr(node, "position", &position_str);        
    // if position string starts with DCS then lookup its actual position on the bus
    if (position_str[0] == 'D' &&
        position_str[1] == 'C' &&
        position_str[2] == 'S') {
        int dcs;                
        if (sscanf(position_str, "DCS%08d", &dcs) != 1) {
            fprintf(stderr, "Can't parse DCS number '%s'\n", position_str);
            exit(1);             
        }
        ELLNODE * lnode;
        for(lnode = ellFirst(&ctx->config->dcs_lookups); lnode; lnode = ellNext(lnode))
        {
            EC_DCS_LOOKUP * dcs_lookup = (EC_DCS_LOOKUP *)lnode;
            if (dcs == dcs_lookup->dcs) {
                ctdev->position = dcs_lookup->position;
                // 255 chars should be enough for a position on the bus!
                char temp_position[255];
                snprintf(temp_position, 255, "%d", dcs_lookup->position);               
                xmlSetProp(node, (unsigned char *) "position", (unsigned char *) temp_position);
            }
        }
    } else { 
        if(isOctal(position_str)) { 
            fprintf(stderr, "error: constants with leading zeros are disallowed as they are parsed as octal\n"); 
            exit(1); 
        } 
        ctdev->position = strtol(position_str, NULL, 0); 
    } 
    if (ctdev->position == -1) {
        fprintf(stderr, "can't find '%s' on bus\n", position_str);
        exit(1);            
    }
    
    dev_name_okay = getStr(node, "name", &ctdev->name);
    dev_type_name_okay = getStr(node, "type_name", &ctdev->type_name);
    dev_revid_okay = getInt(node, "revision", &ctdev->type_revid, 1);
    joinOkay = joinDevice(ctx->config, ctdev);
    printf("parseDevice: name %s type_name %s rev 0x%x\n", 
           ctdev->name, ctdev->type_name, ctdev->type_revid);
    return 
        dev_name_okay && dev_type_name_okay && dev_revid_okay && 
        getInt(node, "oversample", &ctdev->oversampling_rate, 0) &&
        joinOkay && parseSimspec(node, ctx) &&
        ellAddOK(&ctx->config->devices, &ctdev->node);
}

int parseChain(xmlNode * node, CONTEXT * ctx)
{
    int r = parseChildren(node, ctx, "device", parseDevice);
    show(ctx);
    return r;
}

int joinPdoEntryMapping(EC_CONFIG * cfg, int pdo_index, EC_PDO_ENTRY_MAPPING * mapping)
{
    assert(mapping->device_position != -1);
    mapping->parent = find_device(cfg, mapping->device_position);
    if(mapping->parent == NULL)
    {
        printf("Can't find %d\n", mapping->device_position);
        return 0;
    }
    mapping->pdo_entry = find_pdo_entry(mapping->parent, pdo_index, 
                                       mapping->index, mapping->sub_index);
    if(mapping->pdo_entry == NULL)
    {
        return 0;
    }
    ellAddOK(&mapping->parent->pdo_entry_mappings, &mapping->node);
    return 1;
}

int parsePdoEntryMapping(xmlNode * node, CONTEXT * ctx)
{
    int pdo_index = 0;
    ctx->pdo_entry_mapping = calloc(1, sizeof(EC_PDO_ENTRY_MAPPING));
    EC_PDO_ENTRY_MAPPING *mp = ctx->pdo_entry_mapping;
    return 
        getInt(node, "index", &mp->index, 1) &&
        getInt(node, "sub_index", &mp->sub_index, 1) &&
        getInt(node, "device_position", &mp->device_position, 1) &&
        getInt(node, "offset", &mp->offset, 1) &&
        getInt(node, "bit", &mp->bit_position, 1) &&
        getInt(node, "pdo_index", &pdo_index, 1) &&
        joinPdoEntryMapping(ctx->config, pdo_index, mp);
}

int parseEntriesFromBuffer(char * text, int size, EC_CONFIG * cfg)
{
    CONTEXT ctx;
    ctx.config = cfg;
    //printf("%s\n", text);
    xmlDoc * doc = xmlReadMemory(text, size, NULL, NULL, 0);
    assert(doc);
    xmlNode * node = xmlDocGetRootElement(doc);
    assert(node);
    parseChildren(node, &ctx, "entry", parsePdoEntryMapping);
    return 0;
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

/*
 * populate an EC_CONFIG from xml document
 *
 * <code>cfg</code> - structure to be populated
 * 
 * <code>config</code> - xml document read in memory
 */
int read_config2(char * config, int size, EC_CONFIG * cfg)
{
    LIBXML_TEST_VERSION;
    cfg->doc = xmlReadMemory(config, size, NULL, NULL, 0);
    assert(cfg->doc);
    CONTEXT ctx;
    ctx.config = cfg;
    xmlNode * node = xmlDocGetRootElement(cfg->doc);
    xmlNode * c;
    for(c = node->children; c; c = c->next)
    {
        if(c->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        printf("%s\n", c->name);
        if(strcmp((char *)c->name, "devices") == 0)
        {
            parseTypes(c, &ctx);
        }
        else if(strcmp((char *)c->name, "chain") == 0)
        {
            parseChain(c, &ctx);
        }
    }
    return 0;
}

char * load_config(char * filename)
{
    struct stat st;
    if(stat(filename, &st) != 0)
    {
        fprintf(stderr, "failed to get size of %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    void * buf = calloc(st.st_size + 1, sizeof(char));
    assert(buf);
    FILE * f = fopen(filename, "r");
    if(f == NULL)
    {
        fprintf(stderr, "failed to open %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    assert(fread(buf, sizeof(char), st.st_size, f) == st.st_size);
    assert(fclose(f) == 0);
    return buf;
}

char * regenerate_chain(EC_CONFIG * cfg)
{
    /* output expanded chain file with modified position values */
    int ecount;
    unsigned char * ebuf;
    xmlDocDumpFormatMemory(cfg->doc, &ebuf, &ecount, 0);
    
    /* alloc space for chain file*/
    char * sbuf = calloc(ecount, sizeof(char));
    
    /* write in the chain file and free it */
    strncat(sbuf, (char *) ebuf, ecount-strlen(sbuf)-1);
    xmlFree(ebuf);
    
    return sbuf;   
}

char * serialize_config(EC_CONFIG * cfg)
{
    /* serialize PDO mapping */
    int scount = 1024*1024;
    char * sbuf = calloc(scount, sizeof(char));
    strncat(sbuf, "<entries>\n", scount-strlen(sbuf)-1);
    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        ELLNODE * node1;
        for(node1 = ellFirst(&device->pdo_entry_mappings); node1; node1 = ellNext(node1))
        {
            EC_PDO_ENTRY_MAPPING * mp = (EC_PDO_ENTRY_MAPPING *)node1;
            assert( mp->pdo_entry );
            char line[1024];
            assert(device->position != -1);
            snprintf(line, sizeof(line), "<entry device_position=\"%d\" "
                      "pdo_index=\"0x%x\" index=\"0x%x\" sub_index=\"0x%x\" "
                      "offset=\"%d\" bit=\"%d\" />\n", 
                     device->position, mp->pdo_entry->parent->index, 
                     mp->index, mp->sub_index, mp->offset, 
                     mp->bit_position);
            strncat(sbuf, line, scount-strlen(sbuf)-1);
        }
    }
    strncat(sbuf, "</entries>\n", scount-strlen(sbuf)-1);
    return sbuf;
}

