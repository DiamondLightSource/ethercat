#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include "commands.h"

char * socket_name = "/tmp/scanner.sock";

void * writer_start(void * usr)
{
    int fd = (int)usr;
    cmd_packet packet = {0};
    double period = 100;
    int n = 0;

    packet.cmd = MONITOR;
    packet.channel = 0;
    write(fd, &packet, sizeof(packet));


    while(1)
    {
        n++;
        packet.cmd = WRITE;
        packet.value = 100 * n;
        packet.value = (int)((1 + sin(2 * M_PI * n / period)) * 8000);
        packet.channel = 1;
        write(fd, &packet, sizeof(packet));
        usleep(1000);
    }
}

void * reader_start(void * usr)
{
    int fd = (int)usr;
    cmd_packet packet = {0};
    while(1)
    {
        ssize_t nbytes = read(fd, &packet, sizeof(packet));
        assert( nbytes == sizeof(packet) );
        printf("result is %s %u\n", command_strings[packet.cmd], 
                (unsigned)packet.value);
    }
    return 0;
}

int main(void)
{
    struct sockaddr_un address;
    int socket_fd;
    size_t address_length;

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
        printf("socket() failed\n");
        return 1;
    }
 
    address.sun_family = AF_UNIX;
    address_length = sizeof(address.sun_family) +
        sprintf(address.sun_path, socket_name);

    if(connect(socket_fd, (struct sockaddr *) &address, address_length) != 0)
    {
        printf("connect() failed\n");
        return 1;
    }

    pthread_t reader_thread;
    pthread_t writer_thread;

    pthread_create(&reader_thread, NULL, reader_start, (void *)socket_fd);
    pthread_create(&writer_thread, NULL, writer_start, (void *)socket_fd);
 
    pause();

    close(socket_fd);

    return 0;
}
