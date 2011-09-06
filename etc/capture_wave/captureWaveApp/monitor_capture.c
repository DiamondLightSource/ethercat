/*monitor_capture.c*/
/* This program accepts a waveform PV which sends I/O interrupt updates and assembles
   a requested number of samples, then writes these to a file
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>


#include "cadef.h"
#include "dbDefs.h"
#include "epicsString.h"
#include "cantProceed.h"

enum collection_state { START = 0, COLLECT, COMPLETE, ERROR };
typedef enum collection_state collection_state_t;
long request_type = DBR_LONG;

/* Structure representing one PV (= channel) */
typedef struct 
{
    char* name;
    chid  chid;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;
    unsigned long reqElems;
    int status;
    void* value;
    epicsTimeStamp tsPreviousC;
    epicsTimeStamp tsPreviousS;
    char firstStampPrinted;
    char onceConnected;
} pv;

/* structure representing a sequence of reads */
struct capture 
{
    pv   the_pv;
    int  batchElements;
    int  batchCount;
    collection_state_t state;
    void *collection_start;
    void *collection_end;
    int  collectionElems;
    int  missingElems;
    char *output_file;
    int  connect_ok;
};
typedef struct capture capture_t;

#define COPY(element, size) \
    assert( fwrite(element, size, 1, f) == 1)
void dumpToFile(capture_t *cap)
{
    char *head = "DATA DUMP FILE v2.0";
    char *tail = "END DATA DUMP FILE v2.0";
    time_t now = time(NULL);

    int len;
    int elem_size;
    FILE *f;
    char * now_str = asctime(localtime(&now));

    assert(cap);
    assert(cap->the_pv.dbrType == cap->the_pv.dbfType);
    elem_size = dbr_size_n(cap->the_pv.dbrType,1);
    f = fopen(cap->output_file, "w");
    assert(f);
    len = strlen(head);
    COPY(&len, sizeof(int) );
    COPY(head, strlen(head) );

    len = strlen(now_str);
    COPY(&len, sizeof(int) );
    COPY(now_str, strlen(now_str) );
    
    len = strlen(cap->the_pv.name);
    COPY(&len, sizeof(int) );
    COPY(cap->the_pv.name, strlen(cap->the_pv.name) );
    COPY(&cap->the_pv.dbfType, sizeof(long) );
    COPY(&cap->the_pv.dbrType, sizeof(long) );
    COPY(&cap->the_pv.nElems, sizeof(long) );
    COPY(&cap->the_pv.reqElems, sizeof(long) );

    COPY(&cap->batchElements, sizeof(int) );
    COPY(&cap->batchCount, sizeof(int) );
    COPY(&cap->collectionElems, sizeof(int) );
    COPY(&cap->missingElems, sizeof(int) );

    COPY(&elem_size, sizeof(int));
    len = dbr_size_n(cap->the_pv.dbrType,cap->collectionElems);
    COPY(cap->collection_start, len);
    len = strlen(tail);
    COPY(&len, sizeof(int) );
    COPY(tail, len);
    fclose(f);
    printf("Data written to %s\n", cap->output_file);
}

static void printChidInfo(chid chid, char *message)
{
    printf("\n%s\n",message);
    printf("pv: %s  type(%d) nelements(%ld) host(%s) size(%d)",
    ca_name(chid),ca_field_type(chid),ca_element_count(chid),
    ca_host_name(chid), dbr_size_n(ca_field_type(chid),1));
    printf(" read(%d) write(%d) state(%d)\n",
    ca_read_access(chid),ca_write_access(chid),ca_state(chid));
}

static void exceptionCallback(struct exception_handler_args args)
{
    chid    chid = args.chid;
    long    stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (chid ? ca_name(chid) : noname);


    if(chid) printChidInfo(chid,"exceptionCallback");
    printf("exceptionCallback stat %s channel %s\n",
        ca_message(stat),channel);
}

static void connectionCallback(struct connection_handler_args args)
{
    chid    chid = args.chid;
    printChidInfo(chid,"connectionCallback");
    capture_t *c = (capture_t *) ca_puser(chid);
    c->the_pv.dbfType = ca_field_type(chid);
    c->the_pv.dbrType = ca_field_type(chid);
    c->the_pv.nElems  = ca_element_count(chid);
    c->connect_ok = 1;
}

static void eventCallback(struct event_handler_args eha)
{
    chid    chid = eha.chid;
    capture_t *c = (capture_t *) eha.usr;
    int     count;
    char *end;

    if(eha.status!=ECA_NORMAL) 
    {
       printChidInfo(chid,"eventCallback");
       c->state = ERROR;
    } 
    
    switch (c->state) 
    {
    case START:
        if (c->connect_ok) 
        {
            assert(c->the_pv.dbfType == request_type);
            assert(c->the_pv.dbrType == request_type);
            assert( eha.type == c->the_pv.dbfType );
            assert(dbr_size_n(c->the_pv.dbfType,1) > 0);
            assert(dbr_size_n(c->the_pv.dbrType,1) > 0);
            c->collection_start = (void *) calloc(1, 
                    dbr_size_n( c->the_pv.dbfType, c->batchElements) );
            assert( c->collection_start );
            c->collection_end = c->collection_start;
            c->state = COLLECT;
        }
        else 
        {
            printf("eventCallback waiting for connect\n");
        }
        break;
    case COMPLETE:
        printChidInfo(chid,"eventCallback complete");
        break;
    case ERROR:
        printChidInfo(chid,"eventCallback error");
         break;
    case COLLECT:
         assert( eha.type == c->the_pv.dbfType );
         if (c->missingElems < eha.count)
            count = c->missingElems;
         else
            count = eha.count;
         memcpy(c->collection_end, eha.dbr, dbr_size_n(eha.type, count));
         c->batchCount ++;
         c->collectionElems += count;
         end = ((char *) c->collection_start) + dbr_size_n(eha.type, c->collectionElems);
         c->collection_end = (void *)end;
         c->missingElems -= count;
         assert(c->collectionElems + c->missingElems == c->batchElements);
         if (c->missingElems == 0 )
            c->state = COMPLETE;
         break;
    default:
        assert( 0 );
    }
}

int main(int argc,char **argv)
{
    int result;

    if (argc != 4)
    {
        printf("usage: %s <pv-name> <number-of-elements> <output-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    
    capture_t *cap =  (capture_t *) calloc(1, sizeof(capture_t) );
    assert( cap);
    cap->the_pv.name  = argv[1];
    cap->output_file  = argv[3];
    cap->batchElements= strtol(argv[2],NULL, 10);
    if (errno == ERANGE ) {
        printf("Number of elements \"%s\" not supported\n", argv[2]);
        exit(EXIT_FAILURE);
    }
    cap->missingElems = cap->batchElements;
    cap->batchCount = 0;
    cap->state = START;

    result = ca_context_create(ca_disable_preemptive_callback);
    assert( result == ECA_NORMAL );

    result = ca_add_exception_event(exceptionCallback,NULL);
    assert( result == ECA_NORMAL );

    result = ca_create_channel(cap->the_pv.name,connectionCallback, 
                              cap,20, &(cap->the_pv.chid) );
    assert( result == ECA_NORMAL );

    
    result = ca_create_subscription(request_type, 0, cap->the_pv.chid, DBE_VALUE, 
               eventCallback, (void *) cap, NULL);
    assert( result == ECA_NORMAL );
    
    while (cap->state != COMPLETE )
    {
       /* printf("current count = %d missing= %d\n", cap->batchCount, cap->missingElems); */
       /* returns ECA_TIMEOUT when successful */
       assert( ca_pend_event(0.1) == ECA_TIMEOUT);
    }
    dumpToFile(cap);
    ca_context_destroy();
    return(0);
}
