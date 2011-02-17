#include <stdio.h>

#include "config.h"

#define LINEREAD_BUFSIZE 200
int read_configuration(channel_map_entry *mapping, int max_channels)
{
    FILE *conf;
    char linebuf[LINEREAD_BUFSIZE];
    char *s;
    int c;
    int linecount = 0;
    int channel;
    unsigned int position;
    unsigned int index;
    int alias;
    conf = fopen(CONFIG_FILE, "r");
    while ( (s = fgets(linebuf, LINEREAD_BUFSIZE, conf)) != NULL )
    {
        linecount++;
        if ( s[0] == '\n' ) {
            continue;
        }
        if ( s[0] == '#' ) {
            /* assume comment */
            continue;
        }
        c = sscanf(s, " %d, %d, %d, %d, 0x%x,  0x%x,  0x%x, 0x%x, %d ",
              &channel,
              (int *) &mapping->assigned,
              &alias,
              &position,
              (unsigned int *) &mapping->vendor_id,
              (unsigned int *) &mapping->product_code,
              &index,
              (unsigned int *) &mapping->subindex,
              (int *) &mapping->bit_length);
        if (c != 9)
        {
            printf("Error reading configuration line %d\n", linecount);
            return 2;
        }
        if ( channel > max_channels)
        {
            printf("Error reading configuration line %d\n", linecount);
            printf("channel number must be smaller than %d\n", max_channels);
            return 1;
        }
        mapping->channel = channel;
        mapping->position = position;
        mapping->index = index;
        mapping->alias = alias;
        mapping++;
    }
    printf("read configuration okay\n");
    mapping->channel = END_CHANNEL;
    fclose(conf);
    return 0;
}

void show_registration(ec_pdo_entry_reg_t *entry)
{
    int n = 0;
    /* copy from channel map */
    while (entry->index != 0 )
    {   
        printf("entry %d ", n);
        printf("alias=%d ",  entry->alias);
        printf("position=%d ", entry->position);
        printf("index=0x%x ", entry->index);
        printf("subindex=%d ", entry->subindex);
        printf("offset=%u ",  *(entry->offset) );
        printf("bit_position=%u \n", *(entry->bit_position));
        entry++;
        n++;
    }
}
