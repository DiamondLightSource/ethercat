#ifndef __MSGSOCK_H__
#define __MSGSOCK_H__

int rtSockReceive(int s, void * buf, int len);
int rtSockSend(int s, const void * buf, int len);

int rtSockCreate(const char * socket_name);
int rtServerSockCreate(const char * socket_name);
int rtServerSockAccept(int sock);

#endif
