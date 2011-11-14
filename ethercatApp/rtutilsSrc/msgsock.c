#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "rtutils.h"
#include "msgsock.h"

/* 
   message:
   uint32_t size
   char data[size]
*/

int sendall(int s, const char *buf, size_t len, int flags);
int recvall(int s, char *buf, size_t len, int flags);

int rtSockReceive(int s, void * buf, int len)
{
    int32_t size;
    if(recvall(s, (char *)&size, sizeof(size), 0) != sizeof(size))
    {
        return -1;
    }
    if(len < size)
    {
        return -2;
    }
    return recvall(s, (char *)buf, size, 0);
}

int rtSockSend(int s, const void * buf, int len)
{
    int32_t size = len;
    if(sendall(s, (const char *)&size, sizeof(size), 0) != sizeof(size))
    {
        return -1;
    }
    if(sendall(s, (char *)buf, len, 0) != len)
    {
        return -2;
    }
    return 0;
}

int sendall(int s, const char *buf, size_t len, int flags)
{
    size_t got = 0;
    do
    {
        /* don't want asynchronous SIGPIPEs on client disconnect */
        ssize_t n = send(s, buf, len, flags | MSG_NOSIGNAL);
        if(n < 0)
        {
            return n;
        }
        buf += n;
        got += n;
    } while(got < len);
    return len;
}

int recvall(int s, char *buf, size_t len, int flags)
{
    size_t got = 0;
    do
    {
        ssize_t n = recv(s, buf, len, flags);
        /* retry if interrupted */
        if(n <= 0 && errno != EINTR)
        {
            return n;
        }
        buf += n;
        got += n;
    } while(got < len);
    return len;
}

int rtSockCreate(const char * socket_name)
{
    struct sockaddr_un address = {0};
    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if(sock == -1)
    {
        return -1;
    }
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socket_name, sizeof(address.sun_path)-1);
    if(connect(sock, (struct sockaddr *) &address, sizeof(address)) == -1)
    {
        fprintf(stderr, "connect to %s failed %s\n", socket_name, strerror(errno));
        return -1;
    }
    return sock;
}

int rtServerSockCreate(const char * socket_name)
{
    int sock;
    struct sockaddr_un address;
    size_t address_length;
    int BACKLOG = 0;

    umask(0);
    
    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    assert(sock >= 0);
    unlink(socket_name);
    address.sun_family = AF_UNIX;
    address_length = sizeof(address.sun_family) +
        sprintf(address.sun_path, socket_name);
    assert(bind(sock, (struct sockaddr *) &address, address_length) == 0);
    assert(listen(sock, BACKLOG) == 0);
    
    return sock;
}

int rtServerSockAccept(int sock)
{
    struct sockaddr_un address;
    socklen_t address_length = sizeof(address);
    return accept(
        sock, (struct sockaddr *) &address, &address_length);
}

