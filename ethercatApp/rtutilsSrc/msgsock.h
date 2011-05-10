#ifndef __MSGSOCK_H__
#define __MSGSOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

int rtSockReceive(int s, void * buf, int len);
int rtSockSend(int s, const void * buf, int len);

int rtSockCreate(const char * socket_name);
int rtServerSockCreate(const char * socket_name);
int rtServerSockAccept(int sock);

struct ENGINE;
typedef struct ENGINE ENGINE;

struct ENGINE
{
    char * send_buffer;
    char * receive_buffer;
    int timeout;
    int max_message;
    int listening;
    int id;
    int (*connect)(ENGINE * client);
    int (*on_connect)(ENGINE * client, int sock);
    int (*receive_message)(ENGINE * client);
    int (*send_message)(ENGINE * client, int size);
    void * usr;
    rtMessageQueueId writectrl;
    rtMessageQueueId readctrl;
    char * path;
};

int server_connect(ENGINE * server);
int client_connect(ENGINE * client);
void engine_start(ENGINE * engine);
ENGINE * new_engine(int max_message);

#ifdef __cplusplus
}
#endif

#endif
