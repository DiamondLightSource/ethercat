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

#ifdef __cplusplus
}
#endif
