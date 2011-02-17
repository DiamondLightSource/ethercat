struct action
{
    int tag;
    int device;
    int channel;
    uint64_t value;
};

enum { MSG_TICK = 0, MSG_WRITE, MSG_CYCLE, MSG_MONITOR, MSG_ASYN, MSG_DATA };

