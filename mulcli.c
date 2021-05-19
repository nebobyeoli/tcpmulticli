// ECHO CLIENT
// + MULTICAST RECEIVE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define BUF_SIZE 1024

void error_handling(char *message)
{
    printf("%s", message);
    exit(0);
}

int main(int argc, char *argv[])
{
    // FD
    int fd, state;
    char buf[BUF_SIZE];

    struct  timeval tr;  // timeval_read
    struct  timeval ts;  // timeval_send
    fd_set  readfds;

    fd = fileno(stdin);


    enum { SEND, RECEIVE } MODE;
    MODE = SEND;
    
    
	int sock;
	char message[BUF_SIZE];
	int str_len;
	struct sockaddr_in serv_addr;

	if(argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}
	
	sock = socket(PF_INET, SOCK_STREAM, 0);   
	if(sock == -1) error_handling("socket() error!");
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

    tr.tv_sec = 0;
    tr.tv_usec = 100000;  // 1000000 usec = 1 sec

    // https://stackoverflow.com/a/2939145
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof tr);
	
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) error_handling("connect() error!");
	else printf("Connected...........\n");


	while(1) 
	{
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // tr.tv_sec = 0;
        // tr.tv_usec = 500;

        ts.tv_sec = 0;
        ts.tv_usec = 500000;

        memset(message, 0, BUF_SIZE);

        // printf(". ");

        // if (FD_ISSET(sock, &readfds))
        // {
            str_len = read(sock, message, BUF_SIZE);
            if (str_len < 0) {
                // printf("\nNo message.\n");
            }
            else {
                printf("\nMessage from server: %s\n", message);
            }
        // }

        // // 500usec
        // if (select(0, &readfds, NULL, NULL, &tr) > 0)
        // {
        //     // READ
        //     str_len = read(sock, message, BUF_SIZE);
        //     printf("\nMessage from server: %s\n", message);
        //     fflush(0);
        // }
        
        fflush(0);

        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // fd 0 = stdin

        printf("Input message(Q to quit): \n");
        fflush(0);

        // 2sec
        if (select(1, &readfds, NULL, NULL, &ts) > 0)
        {
            // INPUT

            fgets(message, BUF_SIZE, stdin);
            if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) break;

            // SEND
            write(sock, message, strlen(message));

            // READ
            str_len = read(sock, message, BUF_SIZE);
            printf("\nYou sent: %s\n", message);
            fflush(0);
        }
        else {
            // printf("Timeout.\n");
            // puts("");
        }

        fflush(0);

    //     // if (!rstate_active) {
    //         switch (state)
    //         {
    //             case -1:
    //                 perror("[select] error: ");
    //                 exit(0);
    //                 break;

    //             case 0:  // TIME OVER
    //                 // printf("\nTIME OVER // SWITCHING TO [%s] MODE\n", MODE ? "SEND" : "RECEIVE");
    //                 // fflush(0);

    //                 // SWITCH MODE
    //                 MODE = MODE ? SEND : RECEIVE;

    //                 // fputs("\n", stdout);
    //                 // fflush(0);

    //                 // close(fd);
    //                 // exit(0);
    //                 // rstate_active = 1;

    //                 break;

                
                
    //             default:

    //                 printf("%s Input message(Q to quit): ", MODE ? "RECEIVE" : "SEND");
    //                 fflush(0);

    //                 // if (MODE == SEND) {
    //                     fgets(message, BUF_SIZE, stdin);
    //                     if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) break;
    //                     write(sock, message, strlen(message));

    //                     // READ
    //                     str_len = read(sock, message, BUF_SIZE);
    //                     printf("Message from server: %s", message);
    //                     fflush(0);
    //                 // }

    //                 // else {  // MODE == RECEIVE
    //                 //     // READ
    //                 //     str_len = read(sock, message, BUF_SIZE);
    //                 //     printf("Message from server: %s", message);
    //                 //     fflush(0);
    //                 // }

    //                 break;
    //         }
    //     }
    }
	
	close(sock);
	return 0;
}
