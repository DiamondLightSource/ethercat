#ifndef _messages_H
#define _messages_H
#include <time.h>               /* for struct timespec */
enum { MSG_TICK = 0, MSG_WRITE = 1, MSG_HEARTBEAT = 2,
       MSG_PDO = 3, MSG_CONFIG = 4,
       MSG_SDO_REQ = 5,
       MSG_SDO_WRITE = 6,
       MSG_SDO_READ =  7};

enum { SLAVE_METADATA_CNT = 3 };

typedef struct
{
    int tag;
    char buffer[1];
} CONFIG_MESSAGE;

typedef struct
{
    int tag;
    int offset;
    int bit_position;
    int bits;
    int value;
} WRITE_MESSAGE;

typedef struct
{
    int tag;
    int tv_sec;
    int tv_nsec;
    int working_counter;
    int wc_state;
    int cycle;
    int size;
    char buffer[1];
} PDO_MESSAGE;

typedef struct
{
    int tag;
    int dummy;
} HEARTBEAT_MESSAGE;

typedef struct
{
    int tag;
    struct timespec ts;
} TIMER_MESSAGE;

typedef struct
{
    int tag;
    int device;
    int index;
    int subindex;
    int bits;
} SDO_REQ_MESSAGE;
typedef struct
{
    int tag;
    int device;
    int index;
    int subindex;
    int bits;
    union {
        char cvalue[4];
        int32_t ivalue;
    } value;
} SDO_WRITE_MESSAGE;

typedef struct
{
    int tag;
    int device;
    int index;
    int subindex;
    int bits;
    int state;
    char value[4];
} SDO_READ_MESSAGE;

union EC_MESSAGE
{
    int tag;
    PDO_MESSAGE pdo;
    WRITE_MESSAGE write;
    HEARTBEAT_MESSAGE heartbeat;
    CONFIG_MESSAGE config;
    TIMER_MESSAGE timer;
    SDO_REQ_MESSAGE sdo_req;
    SDO_WRITE_MESSAGE sdo_write;
    SDO_READ_MESSAGE sdo;
};

typedef union EC_MESSAGE EC_MESSAGE;

#endif
