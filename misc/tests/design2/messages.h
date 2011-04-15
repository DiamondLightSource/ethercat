enum { MSG_TICK = 0, MSG_WRITE = 1, MSG_HEARTBEAT = 2, MSG_PDO = 3, MSG_CONFIG = 4 };

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

union EC_MESSAGE
{
    int tag;
    PDO_MESSAGE pdo;
    WRITE_MESSAGE write;
    HEARTBEAT_MESSAGE heartbeat;
    CONFIG_MESSAGE config;
    TIMER_MESSAGE timer;
};

typedef union EC_MESSAGE EC_MESSAGE;
