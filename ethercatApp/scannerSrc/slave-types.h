#ifndef _SLAVE_TYPES_H_
#define _SLAVE_TYPES_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define YES 1
#define NO 0
#define MAX_SLAVE_STRLEN 81
#define BACKUP_LIST_FILE "/home/rjq35657/R3.14.12.3/support/" \
                  "ethercat/etc/scripts/slave-types.txt"
#define SLAVE_TYPES_MAXCOUNT 500
#define NI9144_NAME "NI 9144"

typedef struct slave
{
    char type[MAX_SLAVE_STRLEN];
    char revstring[MAX_SLAVE_STRLEN];
    uint32_t revision;
}
slave_t;

char *read_slave_types(char * slave_list_filename);
int copy_section(char *dest, char * start, char * end);
int get_slave_type(slave_t *slave, char *slave_list_filename);
void read_valid_slaves(char *slave_list_filename);
int set_slave_list(char *slave_list);
int check_valid_slave(char *name, int32_t revision);
char *shorten_name(char *name);
extern slave_t *valid_slaves[SLAVE_TYPES_MAXCOUNT];
extern int valid_slaves_count;

#endif
