// TCP SERVER
// CAN HANDLE MULTIPLE CLIENTS
// iomux.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define MAX_BUF     1024
#define MAX_NAMLEN  30
#define MAX_SOCK    100

void error_handling(char *message)
{
    perror(message);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    int serv_sockfd, clnt_sockfd, sockfd, state;
    struct sockaddr_in clnt_addr, serv_addr;

    struct timeval tv;
    fd_set readfds, otherfds, allfds;
    
    char buf[255];
    int client[MAX_SOCK];
    char names[MAX_SOCK][MAX_NAMLEN];
    int i, j, clnt_sz, clnt_max, fd_max;

    state = 0;

    serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sockfd == -1) error_handling("socket() error");

    memset(&serv_addr, 0x00, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    state = bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (state == -1) error_handling("bind() error");

    state = listen(serv_sockfd, 5);
    if (state == -1) error_handling("listen() error");

    clnt_sockfd = serv_sockfd;

    // INITIALIZE
    clnt_max = -1;
    fd_max = serv_sockfd;

    for (i = 0; i < MAX_SOCK; i++) client[i] = -1;

    FD_ZERO(&readfds);
    FD_SET(serv_sockfd, &readfds);

    printf("\nStarted TCP Server.\n\n\n");
    fflush(stdout);

    while (1)
    {
        allfds = readfds;
        state = select(fd_max + 1, &allfds, 0, 0, NULL);

        // ACCEPT CLIENT TO SERVER SOCK
        if (FD_ISSET(serv_sockfd, &allfds))
        {
            clnt_sz = sizeof(clnt_addr);
            clnt_sockfd = accept(serv_sockfd, (struct sockaddr*)&clnt_addr, &clnt_sz);
            printf("Connection from (%s, %d)\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

            for (i = 0; i < MAX_SOCK; i++)
            {
                if (client[i] < 0) {
                    client[i] = clnt_sockfd;
                    printf("Client number: %d\n", i + 1);
                    printf("Client FD: %d\n", clnt_sockfd);
                    break;
                }
            }

            printf("Accepted [%d]\n", clnt_sockfd);
            printf("===================================\n");

            if (i == MAX_SOCK) perror("Too many clients!\n");

            FD_SET(clnt_sockfd, &readfds);

            if (clnt_sockfd > fd_max)   fd_max = clnt_sockfd;
            if (i > clnt_max)           clnt_max = i;
            if (--state <= 0)           continue;
        }

        // CHECK CLIENT SOCKET
        for (i = 0; i <= clnt_max; i++)
        {
            if ((sockfd = client[i]) < 0) continue;

            if (FD_ISSET(sockfd, &allfds))
            {
                memset(buf, 0, MAX_BUF);

                // DISCONNECT CLIENT
                if (read(sockfd, buf, MAX_BUF) <= 0)
                {
                    printf("Close sockfd : %d\n", sockfd);
                    printf("===================================\n");
                    close(sockfd);
                    FD_CLR(sockfd, &readfds);
                    client[i] = -1;
                    memset(names[i], -1, MAX_NAMLEN * sizeof(int));
                }

                else {
                    printf("RECEIVE [%d]: %s\n", sockfd, buf);

                    // SEND RECEIVED MESSAGE TO ORIGINAL SOCKFD
                    write(sockfd, buf, MAX_BUF);
                }

                if (--state <= 0) break;
            }
        }
    }
}
