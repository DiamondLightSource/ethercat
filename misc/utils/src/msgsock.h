#ifndef __MSGSOCK_H__
#define __MSGSOCK_H__

int rtSockReceive(int s, void * buf, unsigned int len);
int rtSockSend(int s, const void * buf, unsigned int len);

int rtSockCreate(char * socket_name);
int rtServerSockCreate(char * socket_name, int backlog);
int rtServerSockAccept(int sock);

#endif
