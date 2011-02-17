#define MAX_MESSAGE 1024

struct write_request
{
    int tag;
    int vaddr;
    int routing;
    char usr[17];
    int64_t value;
};

struct monitor_request
{
    int tag;
    int vaddr;
    int routing;
    char usr[17];
    ELLNODE node;
};

struct monitor_response
{
    int tag;
    int vaddr;
    int routing;
    uint64_t value;
};

struct monitor_response_wf
{
    int tag;
    int vaddr;
    int routing;
    int length;
    uint64_t value;
};

enum { MSG_EMPTY = 0, MSG_MONITOR, MSG_REPLY, MSG_TICK, MSG_CONNECT, MSG_WRITE };
