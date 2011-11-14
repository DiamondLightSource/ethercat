#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "rtutils.h"
#include "msgsock.h"

static int send_config_message(ENGINE * server, int sock)
{
    int config = 1234;
    rtSockSend(sock, &config, sizeof(int));
    return 0;
}

static int receive_config_message(ENGINE * client, int sock)
{
    rtSockReceive(sock, client->receive_buffer, client->max_message);
    int * words = (int *)client->receive_buffer;
    printf("got config %d\n", words[0]);
    return 0;
}

static int count = 0;
static int seq = 0;

static int server_receive_message(ENGINE * server)
{
    usleep(100000);
    memcpy(server->send_buffer, &seq, sizeof(int));
    seq+=2;
    //printf("msg queue returned %d %d\n", server->id, ack);
    return sizeof(int);
}

static int client_receive_message(ENGINE * server)
{
    usleep(100000);
    memcpy(server->send_buffer, &count, sizeof(int));
    //printf("msg queue returned %d %d\n", server->id, count);
    count++;
    if(count == 10)
    {
        count = 0;
        //server->testquit = 1;
    }
    return sizeof(int);
}

static int show_message(ENGINE * server, int size)
{
    int * words = (int *)server->receive_buffer;
    printf("got some stuff %d %d\n", server->id, words[0]);
    return 0;
}

static int connect_pair(ENGINE * server)
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    //assert(setsockopt(server->listening, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval)) == 0);
    //assert(setsockopt(server->listening, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) == 0);
    return server->listening;
}

int main()
{

    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);

    int max_message = 1000;
    ENGINE * server = new_engine(max_message);
    server->id = 1;
    server->listening = pair[0];
    server->on_connect = send_config_message;
    server->receive_message = server_receive_message;
    server->send_message = show_message;
    server->connect = connect_pair;
    engine_start(server);

    ENGINE * client = new_engine(max_message);
    client->listening = pair[1];
    client->id = 2;
    client->on_connect = receive_config_message;
    client->receive_message = client_receive_message;
    client->send_message = show_message;
    client->connect = connect_pair;
    engine_start(client);

    pause();
    
    return 0;
}

/*
  Timeout causes EAGAIN on send or receive
  Stevens book for socket programming (very useful)
  put in rtutils
*/
