#ifndef _classes_H_
#define _classes_H_

#include <libxml/parser.h>
#include <ellLib.h>

#include "ecrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EC_CONFIG EC_CONFIG;
typedef struct EC_PDO_ENTRY EC_PDO_ENTRY;
typedef struct EC_PDO EC_PDO;
typedef struct EC_SYNC_MANAGER EC_SYNC_MANAGER;
typedef struct EC_DEVICE_TYPE EC_DEVICE_TYPE;
typedef struct EC_DEVICE EC_DEVICE;
typedef struct EC_PDO_ENTRY_MAPPING EC_PDO_ENTRY_MAPPING;
typedef struct EC_DCS_LOOKUP EC_DCS_LOOKUP;
typedef struct EC_SDO EC_SDO;
typedef struct EC_SDO_ENTRY EC_SDO_ENTRY;

// simulation types st_
enum st_type {
    ST_CONSTANT, ST_SQUAREWAVE, ST_SINEWAVE, ST_RAMP, ST_INVALID
};
struct stp_const {
    double value;
};
struct stp_ramp {
    double low;
    double high;
    double period_ms;
    double symmetry;
};
struct stp_sine {
    double low;
    double high;
    double period_ms;
};
struct stp_square {
    double low;
    double high;
    double period_ms;
};
typedef union st_param {
    struct stp_const pconst;
    struct stp_sine psine;
    struct stp_square psquare;
    struct stp_ramp pramp;
} st_param;
typedef struct st_simspec {
   ELLNODE node;
   EC_DEVICE * parent;
   int signal_no;
   int bit_length;
   enum st_type type;
   union st_param params;
} st_simspec;
typedef struct st_signal {
   st_simspec *signalspec;
   void *perioddata;
   int no_samples;
   int index;
} st_signal;

struct EC_PDO_ENTRY
{
    ELLNODE node;
    char * name;
    int index;
    int sub_index;
    int bits;
    int oversampling;
    EC_PDO * parent;
    char * datatype;
    int parameter; // for asyn connection
};

struct EC_PDO
{
    ELLNODE node;
    char * name;
    int index;
    EC_SYNC_MANAGER * parent;
    ELLLIST pdo_entries;
};

struct EC_SYNC_MANAGER
{
    ELLNODE node;
    int index;
    int direction;
    int watchdog;
    EC_DEVICE_TYPE * parent;
    ELLLIST pdos;
};

struct EC_DEVICE_TYPE
{
    ELLNODE node;
    char * name;
    int vendor_id;
    int product_id;
    int revision_id;
    int oversampling_activate;
    ELLLIST sync_managers;
};

struct EC_DEVICE
{
    ELLNODE node;
    char * name;              /*< asyn port name, e.g. ERIO.1*/
    char * type_name;         /*< device type name, e.g. EL3602 */
    int type_revid;           /*< dev revision, e.g. 0x00120000 */
    int position;             /*< position in the bus */
    char * dcs_number;        /*< position encoded as DCS number (if any) */
    int oversampling_rate;    
    ELLLIST pdo_entry_mappings;
    // lookup device type by name
    EC_DEVICE_TYPE * device_type;
    ELLLIST simspecs;
    ELLLIST sdo_requests;     /*< list of sdo entries */
};

enum EC_SDO_PROC_STATE { 
    SDO_PROC_IDLE, 
    SDO_PROC_REQ, 
    SDO_PROC_READ, 
    SDO_PROC_SEND,
    SDO_PROC_WRITEREQ,
    SDO_PROC_WRITE
};

struct EC_SDO
{
    ELLNODE node;
    char * name;
    EC_DEVICE * slave;
    int index;                  /*< sdo index */
    ELLLIST sdoentries;
};
typedef  union  {
    char data[4];
    uint16_t data16;
    uint8_t data8;
} sdodata_t;

struct EC_SDO_ENTRY
{
    ELLNODE node;
    char * description;
    EC_SDO * parent;
    int subindex;               /*< sdo subindex */
    int bits;           /*< sdo size in bits */
    char * asynparameter;       /*< parameter name for the asyn port */
    char * desc;
    ec_sdo_request_t * sdo_request;         /*< sdo request struct from ethercat */
    ec_request_state_t state;
    ec_request_state_t oldstate;
    enum EC_SDO_PROC_STATE sdostate;
    
    sdodata_t sdodata;
    int param_val;              /* three parameters used in asyn port */
    int param_stat;
    int param_trig;
    void *readmsg;              /* opaque pointer to hold a sdo_read_message struct */
    void *writemsg;             /* opaque pointer to hold an sdo_write message  */
};

/*
   [2013-09-16 Mon 12:50:22 ] Adding "shift" field to support EL3602,
   the only 24 bit module that has its raw value sent as the top 24
   bits of a 32 bit word.  For this slaves value field, "shift" will
   be set to "8" to divide by 256 before passing to the asyn layer.
 */
struct EC_PDO_ENTRY_MAPPING
{
    ELLNODE node;
    int offset;
    int bit_position;
    EC_PDO_ENTRY * pdo_entry;
    EC_DEVICE * parent;
    int index;
    int sub_index;
    int device_position;
    int shift;              /*< count of bits to right shift */
    st_signal *sim_signal;
    char *paramname;
};

struct EC_CONFIG
{
    ELLLIST device_types;       /* entries in xpath //devices/device */
    ELLLIST devices;            /* entries in xpath //chain/device */
    ELLLIST pdo_entry_mappings;
    ELLLIST dcs_lookups;
    ELLLIST sdo_requests;
    xmlDoc * doc;
};

struct EC_DCS_LOOKUP
{
    ELLNODE node;
    int position;
    int dcs;
    EC_DEVICE * device;
};

EC_DEVICE * find_device(EC_CONFIG * cfg, int position);
EC_PDO_ENTRY * find_pdo_entry(EC_DEVICE * device, int pdo_index, 
                              int index, int sub_index);
EC_DEVICE_TYPE * find_device_type(EC_CONFIG * cfg, char * type_name, 
                                      int revision_id);
EC_PDO_ENTRY_MAPPING * find_mapping(EC_DEVICE * device, int signal_no, 
                                        int bit_length);

#define INT_24BIT_MAX 8388607
#define INT_24BIT_MIN -8388608


enum parsing_result_type {
    PARSING_ERROR, PARSING_OKAY
};
typedef enum parsing_result_type parsing_result_type_t;

// defined in parser.c
parsing_result_type_t isOctal(char * attr);
char * format(const char *fmt, ...);
#ifdef __cplusplus
}
#endif

#endif
