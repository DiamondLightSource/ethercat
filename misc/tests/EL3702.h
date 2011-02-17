#define Beckhoff 0x00000002
#define EL3702   0x0e763052

struct EL3702_regs
{
    uint16_t cycle[2];
    int16_t value[2][100];
};

typedef struct EL3702_regs EL3702_regs;

struct EL3702_device
{
    ethercat_device self;
    int cycle[2];
    int value[2][100];
    EL3702_regs regs;
    int factor;
};

typedef struct EL3702_device EL3702_device;

ethercat_device * EL3702_init(
    ec_master_t * master, ec_domain_t * domain,
    int alias, int pos, char * usr);

