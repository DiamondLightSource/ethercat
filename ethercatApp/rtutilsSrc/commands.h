#include <stdint.h>

struct cmd_packet
{
    uint32_t cmd;
    uint16_t channel; /* mapped in the server to position, index, subindex, etc*/
    uint64_t value;
};

typedef struct cmd_packet cmd_packet;

struct cmd_scanner
{
    uint8_t cmd;
    uint16_t channel;
    uint8_t bit_length;
    uint32_t offset;
    uint64_t value;
    uint32_t client;
};

typedef struct cmd_scanner cmd_scanner;

char * command_strings[] = {"NOP", "READ", "WRITE", "ACK", "KILL", "MONITOR", "DISCONN"};

enum commands
{
    NOP = 0,
    READ = 1,
    WRITE = 2,
    ACK = 3,
    KILL = 4,
    MONITOR = 5,
    DISCONN = 6
};
