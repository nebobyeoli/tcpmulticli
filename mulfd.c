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

#define BUF_SIZE        1024 * 2
#define CMDCODE_SIZE    4
#define NAME_SIZE       30
#define MAX_SOCKS       100

int client[MAX_SOCKS];
char names[MAX_SOCKS][NAME_SIZE];

char myh[] = "⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀\n⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀\n⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀\n⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀\n⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀\n⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀\n⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀\n⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆\n⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦\n⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷\n⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹\n⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿\n⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷";

void error_handling(char *message)
{
    perror(message);
    exit(0);
}

// SEND TO ALL CLIENTS
void sendAll(int clnt_cnt, int cmdcode, char *sender, char *msg, char *servlog)
{
    int i;
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    
    // APPEND CMDCODE
    sprintf(message, "%d", cmdcode);
    // APPEND NAME OF SENDER
    sprintf(&message[CMDCODE_SIZE + 1], "%s", sender);
    // APPEND MESSAGE
    sprintf(&message[CMDCODE_SIZE + NAME_SIZE + 2], "%s", msg);
    
    if (servlog) printf("\nMESSAGE FROM SERVER: %s\n", servlog);

    printf("Total msgsize: %d of %d maximum\n", CMDCODE_SIZE + NAME_SIZE + strlen(msg), BUF_SIZE);

    for (i = 0; i < clnt_cnt; i++)
    {
        // DISCARD DISCONNECTED OR NAMELESS CLIENTS
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], message, BUF_SIZE);
        printf("Sent to client [%d] (%s)\n", client[i], names[i]);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    int serv_sock, clnt_sock, state;
    struct sockaddr_in clnt_addr, serv_addr;

    struct timeval tv;
    fd_set readfds, otherfds, allfds;
    
    char buf[BUF_SIZE], message[BUF_SIZE];
    char serv_name[NAME_SIZE] = "SERVER";
    int i, j, clnt_size, clnt_cnt, fd_max;

    state = 0;

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_addr, 0x00, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    state = bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (state == -1) error_handling("bind() error");

    state = listen(serv_sock, 5);
    if (state == -1) error_handling("listen() error");

    clnt_sock = serv_sock;

    // INITIALIZE
    clnt_cnt = -1;
    fd_max = serv_sock;

    for (i = 0; i < MAX_SOCKS; i++) client[i] = -1;

    FD_ZERO(&readfds);
    FD_SET(serv_sock, &readfds);

    printf("\nStarted TCP Server.\n\n\n");
    fflush(stdout);

    while (1)
    {
        allfds = readfds;
        state = select(fd_max + 1, &allfds, 0, 0, NULL);

        // ACCEPT CLIENT TO SERVER SOCK
        if (FD_ISSET(serv_sock, &allfds))
        {
            clnt_size = sizeof(clnt_addr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_size);
            printf("Connection from (%s, %d)\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

            for (i = 0; i < MAX_SOCKS; i++)
            {
                if (client[i] < 0) {
                    client[i] = clnt_sock;
                    printf("Client number: %d\n", i + 1);
                    printf("Client FD: %d\n", clnt_sock);
                    break;
                }
            }

            printf("Accepted [%d]\n", clnt_sock);
            printf("===================================\n");

            if (i == MAX_SOCKS) perror("Too many clients!\n");

            FD_SET(clnt_sock, &readfds);
            memset(names[i], 0, NAME_SIZE);

            if (clnt_sock > fd_max) fd_max = clnt_sock;
            if (i + 1 > clnt_cnt)   clnt_cnt = i + 1;
            if (--state <= 0)       continue;
        }

        // CHECK CLIENT SOCKET
        for (i = 0; i < clnt_cnt; i++)
        {
            if (client[i] < 0) continue;

            if (FD_ISSET(client[i], &allfds))
            {
                memset(buf, 0, BUF_SIZE);

                // DISCONNECT CLIENT
                if (read(client[i], buf, BUF_SIZE) <= 0)
                {
                    printf("Disconnected client [%d] (%s)\n", client[i], names[i]);
                    printf("===================================\n");
                    fflush(0);

                    close(client[i]);
                    FD_CLR(client[i], &readfds);
                    client[i] = -1;

                    // IF NAME WAS SET (HAD ACTUALLY ENGAGED IN CHAT)
                    if (names[i][0])
                    {
                        // SEND DISCONNECT INFORMATION TO ALL CLIENTS
                        memset(message, 0, BUF_SIZE);
                        sprintf(message, "%s left the chat.", names[i]);
                        sendAll(clnt_cnt, 1000, serv_name, message, message);

                        memset(names[i], 0, NAME_SIZE);
                    }
                }

                else
                {
                    int cmdcode = atoi(buf);

                    printf("\nReceived from [%d] (%s): %s %s\n", client[i], names[i], buf, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);

                    // MODE: SET NAME
                    if (cmdcode == 2000)
                    {
                        // CHECK IF REQUESTED NAME IS TAKEN
                        int taken = 0;
                        for (j = 0; !taken && j < clnt_cnt; j++)
                            if (!strcmp(&buf[CMDCODE_SIZE + 1], names[j])) taken = 1;

                        if (taken) {
                            write(client[i], "0", 2);  // REJECTED
                        }
                        else {
                            write(client[i], "1", 2);  // ACCEPTED

                            memset(names[i], 0, NAME_SIZE);
                            sprintf(names[i], "%s", &buf[CMDCODE_SIZE + 1]);
                            printf("Set name of client [%d] as [%s]\n", client[i], names[i]);

                            // SEND JOIN INFORMATION TO ALL CLIENTS
                            memset(message, 0, BUF_SIZE);
                            sprintf(message, "%s joined the chat!", names[i]);
                            sendAll(clnt_cnt, 1000, serv_name, message, message);
                        }
                    }

                    // MODE: MESSAGE
                    else if (cmdcode == 3000)
                    {
                        char msg[BUF_SIZE], mdest[BUF_SIZE];
                        strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);

                        // CHECK FOR EMOJIS
                        char *index = strstr(msg, ":myh:");
                        if (index)
                        {
                            int betw = (int)index - (int)msg;
                            memcpy(mdest, msg, betw);
                            sprintf(&mdest[betw], "\n%s\n%s", myh, &msg[betw + sizeof(":myh:") - 1]);
                            printf("%s\n%s\n", msg, mdest);
                            fflush(stdout);
                        }

                        // SEND RECEIVED MESSAGE TO ALL CLIENTS
                        memset(message, 0, BUF_SIZE);
                        sendAll(clnt_cnt, 3000, names[i], index ? mdest : msg, NULL);
                    }

                    else
                    {
                        printf("Error reading cmdcode from [%d]!\n", client[i]);
                    }
                }

                if (--state <= 0) break;
            }
        }
    }
}
