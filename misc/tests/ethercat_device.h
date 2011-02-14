enum { PERIOD_NS = 1000000 };

// this is the asyn-style decomposed message
// request is ("value0")
// reply is one of these messages

typedef struct
{
    int tag;
    int reason;
    uint64_t value;
} value_t;

typedef struct
{
    char * name;
    int offset;
    int size;
} field_t;

typedef struct ethercat_device ethercat_device;

struct ethercat_device
{
    void (*write)(ethercat_device * self, int channel, uint64_t value);
    void (*process)(ethercat_device * self, uint8_t * pd);
    void (*show)(ethercat_device * self);
    field_t * (*get_field)(ethercat_device * self, char * name);
    int (*read_field)(ethercat_device * self, field_t * field, void * buf, int max_buf);
    field_t * fields;
    int alias;
    int pos;
    char * usr;
    uint32_t vendor;
    uint32_t product;
    uint8_t alarm;
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
};

field_t * ethercat_device_get_field(ethercat_device * self, char * name);
int ethercat_device_read_field(ethercat_device * self, field_t * f, void * buf, int max_buf);
