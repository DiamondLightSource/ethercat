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
#include "ecrt.h"
#include "classes.h"

enum parser_dir_type {
  /* mirrors ec_direction_t */
  PARSER_DIR_INVALID, PARSER_DIR_OUTPUT, PARSER_DIR_INPUT
};

enum parser_required {
    PARSER_OPTIONAL, PARSER_REQUIRED
};


parsing_result_type_t ellAddOK(ELLLIST * pList, ELLNODE * pNode)
{
    ellAdd(pList, pNode);
    return PARSING_OKAY;
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
    EC_SDO * sdo;
    EC_SDO_ENTRY * sdo_entry;
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

parsing_result_type_t expectNode(xmlNode * node, char * name)
{
    if((node->type == XML_ELEMENT_NODE) &&
       (strcmp((char *)node->name, name) == 0))
    {
        return PARSING_OKAY;
    }
    else
    {
        printf("expected %s got %s", node->name, name);
        return PARSING_ERROR;
    }
}

typedef parsing_result_type_t (*PARSEFUNC)(xmlNode *, CONTEXT *);

parsing_result_type_t parseChildren(xmlNode * node, CONTEXT * ctx, char * name, PARSEFUNC parseFunc)
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
            return PARSING_ERROR;
        }
    }
    return PARSING_OKAY;
}

// octal constants are an absolute disaster, check them here
parsing_result_type_t isOctal(char * attr)
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
            return PARSING_OKAY;
        }
        else
        {
            break;
        }
    }
    return PARSING_ERROR;
}

parsing_result_type_t getDouble(xmlNode * node, char * name, double * value)
{
    char * text = (char *) xmlGetProp(node, (unsigned char *)name);
    char * end_str;
    if (text != NULL)
    {
        *value = strtod(text, &end_str);
        if ( *end_str == 0 )
            return PARSING_OKAY;
    }
    printf("getDouble: Could not find parameter %s\n", name);
    return PARSING_ERROR;
}

parsing_result_type_t getInt(xmlNode * node, char * name, int * value, int required)
{
    char * text = (char *)xmlGetProp(node, (unsigned char *)name);
    if(text != NULL)
    {
        if(isOctal(text))
        {
            printf("error: constants with leading zeros are disallowed as they are parsed as octal\n");
            return PARSING_ERROR;
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
            return PARSING_OKAY;
        }
    }
    return PARSING_ERROR;
}

parsing_result_type_t getStr(xmlNode * node, char * name, char ** value)
{
    *value = (char *)xmlGetProp(node, (unsigned char *)name);
    if (*value == NULL)
        return PARSING_ERROR;
    return PARSING_OKAY;
}

parsing_result_type_t parsePdoEntry(xmlNode * node, CONTEXT * ctx)
{
    ctx->pdo_entry = calloc(1, sizeof(EC_PDO_ENTRY));
    return 
        getInt(node, "index", &ctx->pdo_entry->index, PARSER_REQUIRED) &&
        getInt(node, "subindex", &ctx->pdo_entry->sub_index, PARSER_REQUIRED) &&
        getInt(node, "bit_length", &ctx->pdo_entry->bits, PARSER_REQUIRED) &&
        getStr(node, "name", &ctx->pdo_entry->name) &&
        getStr(node, "datatype", &ctx->pdo_entry->datatype) &&
        getInt(node, "oversample", &ctx->pdo_entry->oversampling, PARSER_REQUIRED) &&
        (ctx->pdo_entry->parent = ctx->pdo) && 
        ellAddOK(&ctx->pdo->pdo_entries, &ctx->pdo_entry->node);
}

parsing_result_type_t parsePdo(xmlNode * node, CONTEXT * ctx)
{
    ctx->pdo = calloc(1, sizeof(EC_PDO));
    return 
        getStr(node, "name", &ctx->pdo->name) &&
        getInt(node, "index", &ctx->pdo->index, PARSER_REQUIRED) &&
        parseChildren(node, ctx, "entry", parsePdoEntry) &&
        (ctx->pdo->parent = ctx->sync_manager) && 
        ellAddOK(&ctx->sync_manager->pdos, &ctx->pdo->node);
}
             
parsing_result_type_t parseSync(xmlNode * node, CONTEXT * ctx)
{
    ctx->sync_manager = calloc(1, sizeof(EC_SYNC_MANAGER));
    EC_SYNC_MANAGER *sm = ctx->sync_manager;
    char parseBuffer[20];
    char *direction = parseBuffer;
    int parsingIsOkay = 1;
    parsingIsOkay = getStr(node, "dir", &direction);
    if (strcasecmp("outputs", direction) == 0)
    {
      sm->direction = PARSER_DIR_OUTPUT;
    }
    else if (strcasecmp("inputs", direction) == 0)
    {
      sm->direction = PARSER_DIR_INPUT;
    }
    else
    {
      sm->direction = PARSER_DIR_INVALID;
    }
    parsingIsOkay =parsingIsOkay &&
        getInt(node, "index", &sm->index, PARSER_REQUIRED) &&
        getInt(node, "watchdog", &sm->watchdog, PARSER_REQUIRED) &&
        parseChildren(node, ctx, "pdo", parsePdo) &&
        (sm->parent = ctx->device_type);
    if (parsingIsOkay)
    {
        if (sm->direction == PARSER_DIR_INVALID)
            return PARSING_OKAY;
        return ellAddOK(&ctx->device_type->sync_managers, &sm->node);
    }
    else
        return PARSING_ERROR;
}

parsing_result_type_t parseDeviceType(xmlNode * node, CONTEXT * ctx)
{
    ctx->device_type = calloc(1, sizeof(EC_DEVICE_TYPE));
    EC_DEVICE_TYPE *dt = ctx->device_type;
    return
        getStr(node, "name", &dt->name) &&
        getInt(node, "dcactivate", 
               &dt->oversampling_activate, PARSER_OPTIONAL) &&
        getInt(node, "vendor", &dt->vendor_id, PARSER_REQUIRED) &&
        getInt(node, "product", &dt->product_id, PARSER_REQUIRED) &&
        getInt(node, "revision", &dt->revision_id, PARSER_REQUIRED) &&
        parseChildren(node, ctx, "sync", parseSync) &&
        ellAddOK(&ctx->config->device_types, &dt->node);
}

parsing_result_type_t parseTypes(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "device", parseDeviceType);
}

/* chain parser */

parsing_result_type_t joinDevice(EC_CONFIG * cfg, EC_DEVICE * dv)
{
    dv->device_type = find_device_type(cfg, dv->type_name, 
                                       dv->type_revid);
    if (dv->device_type != NULL)
        return PARSING_OKAY;
    return PARSING_ERROR;
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

parsing_result_type_t parseParams(xmlNode * node, st_simspec * spec)
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
            return PARSING_ERROR;
    }
    return PARSING_ERROR;
}

parsing_result_type_t find_duplicate(CONTEXT * ctx, int signal_no, int bit_length)
{
    ELLNODE * node = ellFirst(&ctx->device->simspecs);
    for ( ; node; node = ellNext(node) )
    {
        st_simspec * simspec = (st_simspec *)node;
        if ( (simspec->signal_no == signal_no) && (simspec->bit_length == bit_length) )
        {
            return PARSING_OKAY;
        }
    }
    return PARSING_ERROR;
}

parsing_result_type_t parseSimulation(xmlNode * node, CONTEXT * ctx)
{
    ctx->simspec = calloc(1, sizeof(st_simspec));
    char *type_str;
    if ( getStr(node, "signal_type", &type_str) 
            && getInt(node, "signal_no", &ctx->simspec->signal_no, PARSER_REQUIRED) 
            && getInt(node, "bit_length", &ctx->simspec->bit_length, PARSER_REQUIRED) )
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
        return PARSING_ERROR;
}

parsing_result_type_t parseSimspec(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "simulation", parseSimulation);
}

parsing_result_type_t parseDevice(xmlNode * node, CONTEXT * ctx)
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
        position_str[2] == 'S') 
      {
        int dcs;                
        if (sscanf(position_str, "DCS%08d", &dcs) != 1) {
            fprintf(stderr, "Can't parse DCS number '%s'\n", position_str);
            exit(1);             
        }
        ELLNODE * lnode = ellFirst(&ctx->config->dcs_lookups);
        int found_slave = 0;
        for(; lnode && !found_slave; lnode = ellNext(lnode))
        {
            EC_DCS_LOOKUP * dcs_lookup = (EC_DCS_LOOKUP *)lnode;
            if (dcs == dcs_lookup->dcs) {
                found_slave = 1;
                ctdev->position = dcs_lookup->position;
                char *temp_position = format("%d", ctdev->position);
                xmlSetProp(node, (unsigned char *) "position",
                           (unsigned char *) temp_position);
                free(temp_position);
                dcs_lookup->device = ctdev;
            }
        }
    } else { 
        if(isOctal(position_str)) { 
            fprintf(stderr, "error: constants with leading zeros are disallowed as they are parsed as octal\n"); 
            exit(1); 
        } 
       
        ctdev->position = strtol(position_str, NULL, 0); 
        ELLNODE * lnode = ellFirst(&ctx->config->dcs_lookups);
        int found_slave = 0;
        for(; lnode && !found_slave; lnode = ellNext(lnode))
        {
            int found_slave = 0;
            EC_DCS_LOOKUP * dcs_lookup = (EC_DCS_LOOKUP *)lnode;
            if (ctdev->position == dcs_lookup->position) 
            {
                found_slave = 1;
                dcs_lookup->device = ctdev;
            }
        }
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

parsing_result_type_t parseChain(xmlNode * node, CONTEXT * ctx)
{
    parsing_result_type_t r = parseChildren(node, ctx, "device", parseDevice);
    /* show(ctx); */
    return r;
}

parsing_result_type_t joinPdoEntryMapping(EC_CONFIG * cfg, int pdo_index, EC_PDO_ENTRY_MAPPING * mapping)
{
    assert(mapping->device_position != -1);
    mapping->parent = find_device(cfg, mapping->device_position);
    if(mapping->parent == NULL)
    {
        printf("Can't find %d\n", mapping->device_position);
        return PARSING_ERROR;
    }
    mapping->pdo_entry = find_pdo_entry(mapping->parent, pdo_index, 
                                       mapping->index, mapping->sub_index);
    if(mapping->pdo_entry == NULL)
    {
        return PARSING_ERROR;
    }
    return ellAddOK(&mapping->parent->pdo_entry_mappings, &mapping->node);
}

parsing_result_type_t parsePdoEntryMapping(xmlNode * node, CONTEXT * ctx)
{
    int pdo_index = 0;
    ctx->pdo_entry_mapping = calloc(1, sizeof(EC_PDO_ENTRY_MAPPING));
    EC_PDO_ENTRY_MAPPING *mp = ctx->pdo_entry_mapping;
    int parsingIsOkay;
    parsingIsOkay = getInt(node, "index", &mp->index, 1) &&
        getInt(node, "sub_index", &mp->sub_index, 1) &&
        getInt(node, "device_position", &mp->device_position, 1) &&
        getInt(node, "offset", &mp->offset, 1) &&
        getInt(node, "bit", &mp->bit_position, 1) &&
        getInt(node, "pdo_index", &pdo_index, 1);

    if (parsingIsOkay && (mp->index == 0) && (mp->sub_index == 0))
    {
        printf("Skipping gap for pdo_index 0x%x\n", pdo_index);
        free(ctx->pdo_entry_mapping);
        return 1;
    }
    else return 
        parsingIsOkay &&
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
    return PARSING_ERROR;
}

parsing_result_type_t joinSlave(EC_CONFIG * cfg, char * slave, 
                                EC_SDO * sdo)
{
    /* fill in the sdo's slave field */
    ELLNODE * node;
    assert(sdo->slave == NULL);
    for (node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE * ) node;
        if (strcmp(device->name, slave)==0)
        {
            sdo->slave = device;
            break;
        }
    }
    if (sdo->slave != NULL)
    {
        /* matched a device */
        
        return PARSING_OKAY;
    }
    return PARSING_ERROR;
}

parsing_result_type_t parseSdoEntry(xmlNode * node, CONTEXT * ctx);

parsing_result_type_t  parseSdo(xmlNode * node, CONTEXT * ctx)
{
    char * slave;
    ctx->sdo = calloc(1, sizeof(EC_SDO));
    parsing_result_type_t parsing_status;
    parsing_status =     getInt(node, "index", &ctx->sdo->index, PARSER_REQUIRED) &&
        getStr(node, "name", &ctx->sdo->name) &&
        getStr(node, "slave", &slave) && 
        joinSlave(ctx->config, slave, ctx->sdo);
    return parsing_status &&
        parseChildren(node, ctx, "sdoentry", parseSdoEntry) &&
        ellAddOK(&ctx->config->sdo_requests, &ctx->sdo->node);
}

parsing_result_type_t parseSdoEntry(xmlNode * node, CONTEXT * ctx)
{
    EC_SDO_ENTRY * e;
    e = ( ctx->sdo_entry = calloc(1, sizeof(EC_SDO_ENTRY)) );
    return
        getInt(node, "subindex", &e->subindex, PARSER_REQUIRED) &&
        getInt(node, "bit_length", &e->bits, PARSER_REQUIRED) &&
        getStr(node, "description", &e->description) &&
        getStr(node, "asynparameter", &e->asynparameter) &&
        (e->parent = ctx->sdo) &&
        ellAddOK(&e->parent->slave->sdo_requests, &e->node) &&
        ellAddOK(&ctx->sdo->sdoentries, &e->node);
}


int parseSdoRequests(xmlNode * node, CONTEXT * ctx)
{
    return parseChildren(node, ctx, "sdo", parseSdo);
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
            return PARSING_ERROR;
        }
    }
    return PARSING_OKAY;
}

/*
 * populate an EC_CONFIG from xml document
 *
 * <code>cfg</code> - structure to be populated
 * 
 * <code>config</code> - xml document read in memory
 */
int read_config(char * config, int size, EC_CONFIG * cfg)
{
    LIBXML_TEST_VERSION;
    cfg->doc = xmlReadMemory(config, size, NULL, NULL, 0);
    assert(cfg->doc);
    CONTEXT ctx;
    ctx.config = cfg;
    xmlNode * node = xmlDocGetRootElement(cfg->doc);
    xmlNode * c;
    parsing_result_type_t parsing_okay = PARSING_OKAY;
    for(c = node->children; c && parsing_okay; c = c->next)
    {
        if(c->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        if(strcmp((char *)c->name, "devices") == 0)
        {
            parsing_okay = parseTypes(c, &ctx);
        }
        else if(strcmp((char *)c->name, "chain") == 0)
        {
            parsing_okay = parseChain(c, &ctx);
        }
        else if(strcmp((char *)c->name, "sdorequests") == 0)
        {
            parsing_okay = parseSdoRequests(c, &ctx);
        }
    }
    return parsing_okay;
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

