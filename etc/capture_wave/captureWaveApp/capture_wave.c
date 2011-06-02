/*caExample.c*/
#define _GNU_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "cadef.h"

struct pv {
    char *name;
    chid chid;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;
    unsigned long reqElems;
    int status;
    void* value;
};
typedef struct pv pv;

#define COPY(element, size) \
    assert( fwrite(element, size, 1, f) == 1)
void dumpToFile(pv *data, char *output_file)
{
    char *head = "DATA DUMP FILE v1.0";
    char *tail = "END DATA DUMP FILE v1.0";
    time_t now = time(NULL);

    int len;
    int elem_size;
    FILE *f;
    char * now_str = asctime(localtime(&now));

    assert(data);
    elem_size = dbr_size_n(data->dbrType,1);
    f = fopen(output_file, "w");
    assert(f);
    len = strlen(head);
    COPY(&len, sizeof(int) );
    COPY(head, strlen(head) );

    len = strlen(now_str);
    COPY(&len, sizeof(int) );
    COPY(now_str, strlen(now_str) );
    
    len = strlen(data->name);
    COPY(&len, sizeof(int) );
    COPY(data->name, strlen(data->name) );

    COPY(&data->dbfType, sizeof(long) );
    COPY(&data->dbrType, sizeof(long) );
    COPY(&data->nElems, sizeof(unsigned long) );
    COPY(&data->reqElems, sizeof(unsigned long) );

    COPY(&elem_size, sizeof(int));
    COPY(data->value, dbr_size_n(data->dbrType,data->reqElems) );
    len = strlen(tail);
    COPY(&len, sizeof(int) );
    COPY(tail, len);
    fclose(f);
    printf("Data written to %s\n", output_file);
}

int main(int argc,char **argv)
{
    pv wave;
    pv nelm;
    int nElem;
    char *output_file;
    
    if (argc < 3)
    {
        printf("Usage: capture_wave <pv_name> <output_file>\n");
        exit(2);
    }

    assert( asprintf( &wave.name, "%s", argv[1]) > 0 );
    assert( asprintf( &nelm.name, "%s.NELM", argv[1]) > 0 );

    printf("Capturing waveform from PV %s\n", wave.name);

    output_file = argv[2];
    printf("Output will be written to %s\n", output_file);

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_create_channel(nelm.name,NULL,NULL,10,&nelm.chid),"ca_create_channel failure");
    SEVCHK(ca_create_channel(wave.name,NULL,NULL,10,&wave.chid),"ca_create_channel failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
    wave.dbrType = DBR_INT;
    nelm.dbrType = DBR_LONG;
    nelm.dbfType = DBF_LONG;
    nelm.nElems = 1;
    nelm.value = &nElem;
    assert(nelm.value);
    SEVCHK(ca_get(nelm.dbrType,nelm.chid,(void *)nelm.value),"ca_get failure");
    SEVCHK(ca_pend_io(10.0),"ca_pend_io failure");
    printf("Number of elements is %d\n", nElem); 
   
    wave.nElems = ca_element_count(wave.chid);
    wave.reqElems = wave.nElems;
    wave.value = calloc(1, dbr_size_n(wave.dbrType, wave.reqElems));
    assert(wave.value);

    SEVCHK(ca_array_get(wave.dbrType, wave.reqElems, wave.chid, wave.value),
        "array get failure!");
    
    SEVCHK(ca_pend_io(10.0),"ca_pend_io failure");
    printf("creating dump file\n");
    dumpToFile(&wave, output_file);
    return(0);
}
