/* Program to test parsing the description xml */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "classes.h"
#include "parser.h"

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

int main()
{
    testdata_t * data = calloc(1, sizeof(testdata_t));
    data->config = calloc(1, sizeof(EC_CONFIG));
    populate_dcslookup(data->config);
    data->configFile = "/home/rjq35657/ex.xml";
    data->config_buffer = load_config(data->configFile);
    data->load_result = read_config(data->config_buffer, 
                                    strlen(data->config_buffer),
                                    data->config);
    if (data->load_result == PARSING_ERROR)
        printf("PARSING_ERROR returned\n");
    return 0;  
}
