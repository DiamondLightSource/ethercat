/*
 * program to test the functions in slave-types.c, that parse a list
 * of valid slave types and revisions
 */

#include <stdio.h>

#include "slave-types.h"

int main()
{
    int i = 0;
    slave_t *curr_slave;
    read_valid_slaves();

    while ( i < valid_slaves_count )
    {
        curr_slave = valid_slaves[i];
        /* printf( " type %s, revision %s\n", val.type, myslave.revstring); */
        printf( " type %s, revision %s, rev 0x%x\n", curr_slave->type,
                curr_slave->revstring, curr_slave->revision);
        ++i;
    }
    return 0;
}
