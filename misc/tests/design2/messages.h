struct action
{
    int tag;
    int device;
    int channel;
    uint64_t value;
};

enum { MSG_TICK = 0, MSG_MONITOR = 1, MSG_REPLY = 2, MSG_WRITE = 3, MSG_DISCONN = 4, MSG_PDO = 5, MSG_CYCLE, MSG_SOCK, MSG_CONFIG };

