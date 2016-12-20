/*
 * Functions to process slave types
 */
#include <assert.h>
#include "slave-types.h"

#define SEPARATOR " rev "
#define LINE_SEPARATOR "\n"

#define ERRMSGFMT "parsing test program could not stat %s or %s"

/* the list of valid slaves is  
   declared in the header, defined in this c file */
slave_t *valid_slaves[SLAVE_TYPES_MAXCOUNT];
int valid_slaves_count;

/* slave_list is the filename of the slave list being used */
static char *slave_list;

/* set slave_list file 
   returns YES if okay, NO if allocation fails */
int set_slave_list(char *slave_list_arg)
{
    if (slave_list!=NULL)
    {
        free(slave_list);
    }
    slave_list = calloc(sizeof(char),strlen(slave_list_arg)+1);
    if (!slave_list)
        return NO;
    strcpy(slave_list, slave_list_arg);
    return YES;
}

char *read_slave_types()
{
    char *buffer;
    char error_msg[250];
    FILE *f;
    char *filename;
    char filename2[] = BACKUP_LIST_FILE;
    struct stat fstat;
    size_t size;
    int result;
    int backup_list = NO;

    if (slave_list==NULL)
        set_slave_list(SLAVE_LIST_FILE);
    filename = slave_list;
    result = stat(slave_list,&fstat);
    if (result)
    {
        backup_list = YES;
        filename = filename2;        
        result = stat(filename2,&fstat);
        if (result) {
            sprintf(error_msg, ERRMSGFMT, slave_list, filename2);
            perror(error_msg);
            exit(EXIT_FAILURE);
        }
    }
    /* read all bytes to memory and pass the result to caller*/
    buffer = calloc(sizeof(char), fstat.st_size);
    f = fopen(filename, "r");
    size = fread(buffer, sizeof(char), fstat.st_size, f);
    if (size != fstat.st_size)
    {
        perror("error reading slave types");
        exit(EXIT_FAILURE);
    }
    if (fclose(f)) {
        perror("error closing slave types file");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

/* returns count of chars copied. */
int copy_section(char *dest, char * start, char * end)
{
    int count = 0;
    while (start != end)
    {
        *dest = *start;
        start++;
        dest++;
        ++count;
    }
    *dest = '\0';
    return count;
}


int get_slave_type(slave_t *slave)
{
    /* returns YES if a new slave was parsed */
    static char * p;
    static char * text;
    static int slave_types_read = NO;
    static int separator_len;
    static int line_separator_len;
    int count;
    char * sep;
    if (!slave_types_read)
    {
        text = read_slave_types();
        p = text;
        slave_types_read = YES;
        separator_len = strlen(SEPARATOR);
        line_separator_len = strlen(LINE_SEPARATOR);
    }
    sep = strstr(p, SEPARATOR);
    if (sep == NULL)
        return NO;
    count = copy_section(slave->type, p, sep);
    p += count + separator_len;
    sep = strstr(p, LINE_SEPARATOR);
    if (sep == NULL)
    {
        return NO;
    }
    count = copy_section(slave->revstring, p, sep);
    p += count + line_separator_len;
    slave->revision = strtol(slave->revstring, NULL, 16);
    return(YES);
}

void read_valid_slaves()
{
    slave_t *curr_slave;
    valid_slaves_count = 0;
    int result;
    do  
    {
        curr_slave = calloc(sizeof(slave_t), 1);
        result = get_slave_type(curr_slave);
        if (result == NO)
        {
            free(curr_slave);
        }
        else
        {
            valid_slaves[valid_slaves_count] = curr_slave;
            ++valid_slaves_count;
        }
        if (valid_slaves_count >= SLAVE_TYPES_MAXCOUNT)
        {
            printf("Too many slave types, only %d read\n",
                   SLAVE_TYPES_MAXCOUNT);
            break;
        }
    } while (result == YES);
}

char *shorten_name(char *name)
{
    char *shortname = calloc(strlen(name) + 1, sizeof(char));
    #define SPACE " "
    char *sep;
    if (strstr(name, NI9144_NAME) == name )
    {
        strcpy(shortname, NI9144_NAME);
    }
    else
    {
        sep = strstr(name, SPACE);
        copy_section(shortname, name, sep);
    }
    return shortname;
}

int check_valid_slave(char *name, int32_t revision)
{
    int i = 0;
    slave_t *curr_slave;
    char *shortname = shorten_name(name);
    while ( i < valid_slaves_count)
    {
        curr_slave = valid_slaves[i];
        if ((revision == curr_slave->revision) &&
            (strcmp(shortname, curr_slave->type) == 0) )
        {
            free(shortname);
            return(YES);
        }
        ++i;
    }
    free(shortname);    
    return(NO);
}
