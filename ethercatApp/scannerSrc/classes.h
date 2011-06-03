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
typedef struct NODE NODE;
typedef struct LIST LIST;

#ifndef INCellLibh
struct NODE
{
    NODE * next;
    NODE * previous;
};

struct LIST
{
    int count;
    NODE node;
};
#else
/*
#define NODE ELLNODE
#define LIST ELLLIST
*/
#endif

NODE * listFirst(LIST * list);
int listAdd(LIST * list, NODE * node);

struct EC_PDO_ENTRY
{
    NODE node;
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
    NODE node;
    char * name;
    int index;
    EC_SYNC_MANAGER * parent;
    LIST pdo_entries;
};

struct EC_SYNC_MANAGER
{
    NODE node;
    int index;
    int direction;
    int watchdog;
    EC_DEVICE_TYPE * parent;
    LIST pdos;
};

struct EC_DEVICE_TYPE
{
    NODE node;
    char * name;
    int vendor_id;
    int product_id;
    int oversampling_activate;
    LIST sync_managers;
};

struct EC_DEVICE
{
    NODE node;
    char * name;
    char * type_name;
    int position;
    int oversampling_rate;
    LIST pdo_entry_mappings;
    // lookup device type by name
    EC_DEVICE_TYPE * device_type;
};

// binary
struct EC_PDO_ENTRY_MAPPING
{
    NODE node;
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
    LIST device_types;
    LIST devices;
    LIST pdo_entry_mappings;
};

EC_DEVICE * find_device(EC_CONFIG * cfg, int position);
EC_PDO_ENTRY * find_pdo_entry(EC_DEVICE * device, int index, int sub_index);
EC_DEVICE_TYPE * find_device_type(EC_CONFIG * cfg, char * name);

#ifdef __cplusplus
}
#endif
