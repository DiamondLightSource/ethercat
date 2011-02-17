enum { PERIOD_NS = 1000000 };

// this is the asyn-style decomposed message
// request is ("value0")
// reply is one of these messages

typedef struct pdo_entry_type pdo_entry_type;
struct pdo_entry_type
{
    char * name;
    int uid;
    int index;
    int subindex;
    int type;
    int dir;
    // oversampling
    int length;
    int step;
    // field mapping
    int buffer_offset;
    int buffer_size;
    // pdo mapping
    unsigned int pdo_offset;
    unsigned int pdo_bit;
    ec_sync_info_t * sync;
    ec_pdo_entry_info_t * entry;
    pdo_entry_type * next;
};

typedef struct
{
    int tag;
    int reason;
    uint64_t value;
} value_t;

typedef struct field_new_t field_new_t;
struct field_new_t
{
    char * name;
    void * buffer;
    int size;
};

typedef struct
{
    char * name;
    int offset;
    int size;
} field_t;

typedef struct ethercat_device ethercat_device;

struct ethercat_device
{
    char * name;
    void (*write)(ethercat_device * self, int channel, uint64_t value);
    void (*process)(ethercat_device * self, uint8_t * pd);
    void (*show)(ethercat_device * self);
    field_t * (*get_field)(ethercat_device * self, char * name);
    int (*read_field)(ethercat_device * self, field_t * field, void * buf, int max_buf);
    field_t * fields;
    field_new_t * fields_new;
    int n_fields;
    int alias;
    int pos;
    char * usr;
    uint32_t vendor;
    uint32_t product;
    uint8_t alarm;
    ec_sync_info_t syncs[EC_MAX_SYNC_MANAGERS];
    ec_slave_info_t slave_info;
    char * pdo_buffer;
    pdo_entry_type * regs;
    ethercat_device * next;
};

typedef struct ethercat_constructor ethercat_constructor;

struct ethercat_constructor
{
    char * name;
    ethercat_device * (*init)(
        ec_master_t * master, ec_domain_t * domain, 
        int alias, int pos, char * usr);
};

typedef struct ethercat_device_config ethercat_device_config;

struct ethercat_device_config
{
    char * name;
    int vaddr;
    int alias;
    int pos;
    char * usr;
    ethercat_device * dev;
    ethercat_device_config * next;
};

enum { S8 = 0, U8, S16, U16, S32, U32 };

field_t * ethercat_device_get_field(ethercat_device * self, char * name);
int ethercat_device_read_field(ethercat_device * self, field_t * f, void * buf, int max_buf);
