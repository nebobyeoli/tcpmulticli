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

#define BUF_SIZE        1024
#define CMDCODE_SIZE    4
#define NAME_SIZE       30

#define RECV_TIMEOUT_SEC    0
#define RECV_TIMEOUT_USEC   100000  // 1000000 usec = 1 sec
#define SEND_TIMEOUT_SEC    0
#define SEND_TIMEOUT_USEC   100000

#define MIN_ERASE_LINES     2
#define PP_LINE_SPACE       2       // min: 1 // prompt print ('Input message~') line space

void error_handling(char *message)
{
    printf("%s", message);
    exit(0);
}

void itoa(int i, char *st) {
    sprintf(st, "%d", i);
    return;
}

// NOTE. this is for LINUX
// https://stackoverflow.com/a/35190285
/* 
 * \33[2K   erases the entire line your cursor is currently on
 *
 * \033[A   moves your cursor up one line, but in the same column
 *          i.e. not to the start of the line
 * 
 * \r       brings your cursor to the beginning of the line
 *          (r is for carriage return, N.B. carriage returns
 *           do not include a newline so cursor remains on the same line)
 *          but does not erase anything
 * 
 * \b       erase 1 char b ack
 */
void moveCursorUp(int lines, int eraselast)
{
    if (eraselast) printf("\33[2K");
    for (int i = 0; i < lines; i++) printf("\033[A\33[2K");
    printf("\r");
    fflush(0);
}

int main(int argc, char *argv[])
{
    char buf[BUF_SIZE], message[BUF_SIZE];

    struct  timeval tr;     // timeval_receive
    struct  timeval ts;     // timeval_send
    fd_set  readfds;        // CONTROLS SELECT

    // fd = fileno(stdin);

    int sock;
    int namelen;
    struct sockaddr_in serv_addr;

    int prompt_printed = 0;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    
    sock = socket(PF_INET, SOCK_STREAM, 0);   
    if (sock == -1)
        error_handling("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    tr.tv_sec  = RECV_TIMEOUT_SEC;
    tr.tv_usec = RECV_TIMEOUT_USEC;

    // https://stackoverflow.com/a/2939145
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof(tr));
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    
    printf("CONNECTED TO SERVER.\n\n");

    int warned = 0;
    while (1)
    {
        // SET NAME
        printf("NICKNAME: ");
        fflush(0);

        fgets(buf, BUF_SIZE, stdin);
        for (namelen = 0; buf[namelen] != '\n'; namelen++);
        buf[namelen] = 0;
        
        if (namelen < 1) moveCursorUp(1, 0);

        else if (buf[0] == ' ') {
            moveCursorUp(warned ? 2 : 1, 0);
            printf("Name must not start with a SPACE character...\n");
            if (!warned) warned = 1;
        }
        
        else if (namelen > NAME_SIZE) {
            moveCursorUp(warned ? 2 : 1, 0);
            printf("Name must be shorter than %d characters!\n", NAME_SIZE);
            if (!warned) warned = 1;
        }

        else {
            sprintf(message, "1000");
            sprintf(&message[CMDCODE_SIZE + 1], "%s", buf);
            
            write(sock, message, CMDCODE_SIZE + 1 + namelen);

            memset(message, 0, BUF_SIZE);
            read(sock, message, BUF_SIZE);

            if (atoi(message) == 0) {
                moveCursorUp(warned ? 2 : 1, 0);
                printf("Sorry, but the name %s is already taken.\n", buf);
                if (!warned) warned = 1;
            }
            
            else {
                printf("Name accepted.\n");
                break;
            }
        }
    }
    
    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\n");

    // MESSAGE COMMUNICATION LOOP
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        ts.tv_sec  = SEND_TIMEOUT_SEC;
        ts.tv_usec = SEND_TIMEOUT_USEC;

        memset(buf, 0, BUF_SIZE);
        memset(message, 0, BUF_SIZE);

        // RECEIVE MESSAGE
        if (read(sock, message, BUF_SIZE) < 0) {
            // printf("\nNo message.\n");
        }
        else {
            // Attempts to retain what the user was typing after msg receival...
            // fgets(buf, BUF_SIZE, stdin);
            // printf("%s\n", buf);
            // write(STDIN_FILENO, stdin, sizeof(stdin));
            
            // Print msg after removing previous lines.
            moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);
            printf("\n%s sent: %s\n", message, &message[NAME_SIZE + 1]);
            prompt_printed = 0;
            fflush(0);
        }

        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // fd 0 = stdin

        // NOT PRINTING PROMPT WHILE WAITING / TYPING
        if (!prompt_printed) {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\n");
            printf("Input message(q/Q to quit): \n");
            // fflush(0);
            prompt_printed = 1;
        }

        // SEND MESSAGE
        if (select(1, &readfds, NULL, NULL, &ts) > 0)
        {
            // INPUT
            fgets(buf, BUF_SIZE, stdin);
            if (!strcmp(buf, "q\n") || !strcmp(buf, "Q\n")) break;

            sprintf(message, "2000");
            sprintf(&message[CMDCODE_SIZE + 1], "%s", buf);

            // SEND
            write(sock, message, sizeof(message));

            // READ
            read(sock, message, BUF_SIZE);
            moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + 1, 0);
            printf("\n%s sent: %s\n", message, &message[NAME_SIZE + 1]);
            fflush(0);
            prompt_printed = 0;
        }
        else
        {
            // TIMEOUT

            // printf("Timeout.\n");
            // puts("");
        }

        fflush(0);
    }
    
    close(sock);
    return 0;
}
