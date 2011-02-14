#include <stdio.h>
#include <unistd.h>
#include "msgq.h"
#include "msgsock.h"

/* echo server */

void * handle_msg(int sock, void * usr)
{
    int msg[1024];
    while(1)
    {
        int n = sock_get(sock, (char *)&msg, sizeof(msg));
        if(n != 0)
        {
            printf("disconnect by %d\n", n);
            break;
        }
        
        printf("got message, size %d, payload %d %d %d %d\n", n, msg[0], msg[1], msg[2], msg[3]);
        
        if(sock_put(sock, (char *)&msg, sizeof(msg)) != 0)
        {
            printf("send failed\n");
        }
    }
    
    close(sock);
    return 0;
}

int main()
{
    sock_server("/tmp/sock", 0, handle_msg, (void *)99);
    printf("world\n");
    return 0;
}
