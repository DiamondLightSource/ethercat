#define Beckhoff 0x00000002
#define EK1101   0x044d2c52

struct EK1101_regs
{
    uint16_t ID;
};

typedef struct EK1101_regs EK1101_regs;

struct EK1101_device
{
    ethercat_device self;
    int ID;
    EK1101_regs regs;
};

typedef struct EK1101_device EK1101_device;

ethercat_device * EK1101_init(
    ec_master_t * master, ec_domain_t * domain,
    int alias, int pos, char * usr);
