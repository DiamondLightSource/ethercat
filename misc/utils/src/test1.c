#include <stdio.h>
#include "msgq.h"
#include "msgsock.h"

int main()
{
    int n;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "hello world");
    int sock = msgsock_client("/tmp/sock");

    struct point_msg point = {{0}, 0};
    struct kv_msg kv = {{0}, 0};

    for(n = 0; n < 100; n++)
    {
        point.x = n * 2;
        point.y = n * 3;
        point.hdr.size = sizeof(point);
        point.hdr.tag = TAG_POINT_MSG;

        kv.key = n;
        kv.value = n * n;
        kv.hdr.size = sizeof(kv);
        kv.hdr.tag = TAG_KV_MSG;

        /*
        sendall(sock, (char *)&point, sizeof(point), 0);
        sendall(sock, (char *)&kv, sizeof(kv), 0);
        */
    }

    return 0;
}
