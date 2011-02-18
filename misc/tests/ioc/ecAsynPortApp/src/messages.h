#define MAX_MESSAGE 1024

struct write_request
{
    int tag;
    int client;
    int vaddr;
    int routing;
    char usr[16];
    int32_t value;
};

struct monitor_request
{
    int tag;
    int client;
    int vaddr;
    int routing;
    int period;
    char usr[16];
};

struct monitor_response
{
    int tag;
    int client;
    int vaddr;
    int routing;
    int length;
    uint32_t value;
};

enum { MSG_TICK = 0, MSG_MONITOR = 1, MSG_REPLY = 2, MSG_WRITE = 3, MSG_CONNECT = 4 };
