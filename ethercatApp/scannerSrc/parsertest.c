/* Program to test parsing the description xml */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "classes.h"
#include "parser.h"

typedef enum _CONFIG_CHECK { CONFIG_ERROR=0,  CONFIG_OKAY=1 } CONFIG_CHECK;
typedef enum _BOOL { FALSE = 0, TRUE = 1} BOOL;

/* typdef enum _CONFIG_CHECK CONFIG_CHECK; */

typedef struct testdata
{
    char * configFile;
    char * config_buffer;
    int load_result;
    EC_CONFIG * config;
} testdata_t;

struct testdcslookup
{
    char * serial;
};

typedef struct testdcslookup testdcslookup_t;
testdcslookup_t lookup_table[] =
{
    { "DCS00023358"}, { "DCS00023420"}, { "DCS00023361"},
    { "DCS00023597"}, { "DCS00023598"}, { "DCS00023599"},
    { "DCS00023600"}, { "DCS00023601"}, { "DCS00023602"},
    { "DCS00022029"}, { "DCS00023427"}, { "DCS00023962"},
    { "DCS00023360"}, { "DCS00023596"}, { "DCS00022045"},
    { "DCS00023426"}, { "DCS00023421"}, { "DCS00023363"},
    { "DCS00023827"}, { "DCS00023606"}, { "DCS00023607"},
    { "DCS00023513"}, { "DCS00023514"}, { "DCS00023515"},
    { "DCS00023516"}, { "DCS00023964"}, { "DCS00023422"},
    { "DCS00023364"}, { "DCS00023829"}, { "DCS00023610"},
    { "DCS00023611"}, { "DCS00023521"}, { "DCS00023522"},
    { "DCS00023523"}, { "DCS00023524"}, { "DCS00023963"},
    { "DCS00023423"}, { "DCS00023382"}, { "DCS00023691"},
    { "DCS00023692"}, { "DCS00023693"}, { "DCS00023694"},
    { "DCS00023695"}, { "DCS00023455"}, { "DCS00024639"},
    { "DCS00022123"}, { "DCS00024779"}, { "DCS00024780"},
    { "DCS00024781"}, { "DCS00023384"}, { "DCS00024545"},
    { "DCS00024546"}, { "DCS00023456"}, { "DCS00023457"},
    { "DCS00022122"}, { "DCS00023424"},
};

int populate_dcslookup(EC_CONFIG *c)
{
    int i;
    int dcs;
    for (i = 0; i < sizeof(lookup_table)/sizeof(lookup_table[0]); i++)
    {
        struct testdcslookup * entry = &lookup_table[i];

        int matches = sscanf(entry->serial, "DCS%08d", &dcs);
        assert(matches == 1);
        EC_DCS_LOOKUP *dcs_lookup = calloc(1, sizeof(EC_DCS_LOOKUP));
        dcs_lookup->position = i;
        dcs_lookup->dcs = dcs;
        ellAdd(&c->dcs_lookups, &dcs_lookup->node);
        printf("position %d\n", i);
    }
    printf("added %d entries\n", c->dcs_lookups.count);
    return 0;
}

BOOL pdoentryOversample(EC_PDO_ENTRY *entry)
{
    BOOL result = FALSE;
    if (entry->oversampling)
    {
        result = TRUE;
    }
    return result;
}

BOOL pdoOversample(EC_PDO *pdo)
{
    BOOL result = FALSE;
    BOOL loop = TRUE;
    ELLNODE * n = ellFirst(&pdo->pdo_entries);
    while (loop && n)
    {
        EC_PDO_ENTRY *e = (EC_PDO_ENTRY *)n;
        if (pdoentryOversample(e))
        {
            result = TRUE;
            loop = FALSE;
        }
        n = ellNext(n);
    }
    return result;
}

CONFIG_CHECK check_syncmanager(EC_SYNC_MANAGER *sm)
{
    CONFIG_CHECK result = CONFIG_OKAY;
    BOOL oversample = FALSE;
    ELLNODE * n = ellFirst(&sm->pdos);
    BOOL loop = TRUE;
    while (loop && n)
    {
        EC_PDO *pdo = (EC_PDO *)n;
        if (oversample && !pdoOversample(pdo))
        {
            /* configuration out of order */
            result = CONFIG_ERROR;
            loop = FALSE;       /* stop the loop */
        }
        if (!oversample && pdoOversample(pdo))
        {
            oversample = TRUE;
        }
        n = ellNext(n);
    }
    return result;
}

CONFIG_CHECK check_devicetype(EC_DEVICE_TYPE *devtype)
{
    CONFIG_CHECK result = CONFIG_OKAY;
    ELLNODE *n = ellFirst(&devtype->sync_managers);
    BOOL loop = TRUE;
    while (loop && n)
    {
        EC_SYNC_MANAGER *syncmanager = (EC_SYNC_MANAGER *) n;
        if (check_syncmanager(syncmanager) == CONFIG_ERROR)
        {
            loop = FALSE;
            result = CONFIG_ERROR;
        }
        n = ellNext(n);
    }
    return result;
}

/* Function to verify that the pdos in all sync managers where there
 * are oversampling registers are placed AFTER the registers where
 * oversample==0 */
CONFIG_CHECK check_config(EC_CONFIG *c)
{
    CONFIG_CHECK result = CONFIG_OKAY;
    ELLNODE *n = ellFirst(&c->device_types);
    while (n)
    {
        EC_DEVICE_TYPE * devtype = (EC_DEVICE_TYPE *) n;
        if (check_devicetype(devtype) == CONFIG_ERROR)
        {
            printf("check_config: Error found devicetype %s\n",
                devtype->name);
            result = CONFIG_ERROR;
        }
        n = ellNext(n);
    }
    return result;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: parsertest <scanner.xml>\n");
        exit(1);
    }
    char *fname = argv[1];
    fprintf(stderr, "fname %s\n", fname);
    testdata_t * data = calloc(1, sizeof(testdata_t));
    data->config = calloc(1, sizeof(EC_CONFIG));
    /* populate_dcslookup(data->config); */
    data->configFile = fname;
    fprintf(stderr, "fname %s\n", data->configFile);
    data->config_buffer = load_config(data->configFile);
    data->load_result = read_config(data->config_buffer, 
                                    strlen(data->config_buffer),
                                    data->config);
    if (check_config(data->config))  /* == CONFIG_OKAY */
    {
        printf("check_config: OKAY\n");
    }
    if (data->load_result == PARSING_ERROR)
        printf("PARSING_ERROR returned\n");
    return 0;  
}
