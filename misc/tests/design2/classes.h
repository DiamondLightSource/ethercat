#define EC_MAX_PDOS 16
#define EC_MAX_PDO_ENTRIES 32
#define EC_MAX_SYNC_MANAGERS 16

typedef struct EC_CONFIG EC_CONFIG;
typedef struct EC_PDO_ENTRY EC_PDO_ENTRY;
typedef struct EC_PDO EC_PDO;
typedef struct EC_SYNC_MANAGER EC_SYNC_MANAGER;
typedef struct EC_DEVICE_TYPE EC_DEVICE_TYPE;
typedef struct EC_DEVICE EC_DEVICE;
typedef struct EC_PDO_ENTRY_MAPPING EC_PDO_ENTRY_MAPPING;
typedef struct NODE NODE;
typedef struct LIST LIST;

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
};

struct EC_PDO
{
    NODE node;
    int index;
    EC_SYNC_MANAGER * parent;
    LIST pdo_entries;
};

struct EC_SYNC_MANAGER
{
    NODE node;
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
    unsigned int bit_position;
    EC_PDO_ENTRY * pdo_entry;
    EC_DEVICE * parent;
    // for serialization
    int ordinal;
    int parent_position;
};

struct EC_CONFIG
{
    LIST device_types;
    LIST devices;
};
