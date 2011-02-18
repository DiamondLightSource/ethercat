#define Beckhoff 0x00000002
#define EL4102   0x10063052

struct EL4102_regs
{
    int16_t output[2];
};

typedef struct EL4102_regs EL4102_regs;

struct EL4102_device
{
    ethercat_device self;
    int output[2];
    EL4102_regs regs;
};

typedef struct EL4102_device EL4102_device;

ethercat_device * EL4102_init(
    ec_master_t * master, ec_domain_t * domain,
    int alias, int pos, char * usr);

