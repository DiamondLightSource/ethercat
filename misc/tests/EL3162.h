#define Beckhoff 0x00000002
#define EL3162   0x0c5a3052

struct EL3162_regs
{
    uint8_t status[2];
    uint16_t value[2];
};

typedef struct EL3162_regs EL3162_regs;

struct EL3162_device
{
    ethercat_device self;
    int status[2];
    int value[2];
    EL3162_regs regs;
};

typedef struct EL3162_device EL3162_device;

ethercat_device * EL3162_init(
    ec_master_t * master, ec_domain_t * domain,
    int alias, int pos, char * usr);
