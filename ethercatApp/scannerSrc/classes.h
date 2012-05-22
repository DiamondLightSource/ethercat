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
    int oversampling_activate;
    ELLLIST sync_managers;
};

struct EC_DEVICE
{
    ELLNODE node;
    char * name;
    char * type_name;
    int position;
    int oversampling_rate;
    ELLLIST pdo_entry_mappings;
    // lookup device type by name
    EC_DEVICE_TYPE * device_type;
    ELLLIST simspecs;
};

// binary
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
    st_signal *sim_signal;
};

struct EC_CONFIG
{
    ELLLIST device_types;
    ELLLIST devices;
    ELLLIST pdo_entry_mappings;
};


EC_DEVICE * find_device(EC_CONFIG * cfg, int position);
EC_PDO_ENTRY * find_pdo_entry(EC_DEVICE * device, int pdo_index, int index, int sub_index);
EC_DEVICE_TYPE * find_device_type(EC_CONFIG * cfg, char * name);
EC_PDO_ENTRY_MAPPING * find_mapping(EC_DEVICE * device, int signal_no, 
                                        int bit_length);

#ifdef __cplusplus
}
#endif
