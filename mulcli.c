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

#define BUF_SIZE        1024 * 2
#define CMDCODE_SIZE    4
#define NAME_SIZE       30

#define RECV_TIMEOUT_SEC    0
#define RECV_TIMEOUT_USEC   100000  // 1000000 usec = 1 sec
#define SEND_TIMEOUT_SEC    0
#define SEND_TIMEOUT_USEC   100000

#define MIN_ERASE_LINES     1
#define PP_LINE_SPACE       2       // min: 1  // prompt print ('Input message~') line space

int sock;
char nname[NAME_SIZE];

void error_handling(char *message)
{
    printf("%s", message);
    exit(0);
}

void itoa(int i, char *st) {
    sprintf(st, "%d", i);
    return;
}

// MESSAGE SENDER
void send_msg(int cmdcode, char *msg)
{
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);

    sprintf(message, "%d", cmdcode);
    sprintf(&message[CMDCODE_SIZE + 1], "%s", nname);
    sprintf(&message[CMDCODE_SIZE + NAME_SIZE + 2], "%s", msg);

    write(sock, message, BUF_SIZE);
}

// MESSAGE RECEIVER: ALSO SETS PARAMETER VALUES TO 0
// RETURNS read() RESULT
int recv_msg(int *cmdcode, char *sender, char *message)
{
    char buf[BUF_SIZE];
    int rresult;

    memset(sender, 0, NAME_SIZE);
    memset(buf, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);

    if ((rresult = read(sock, buf, BUF_SIZE)) < 0) return rresult;

    // RETRIEVE CMDCODE
    *cmdcode = atoi(buf);
    // RETRIEVE NAME OF SENDER
    sprintf(sender, "%s", &buf[CMDCODE_SIZE + 1]);
    // RETRIEVE MESSAGE
    sprintf(message, "%s", &buf[CMDCODE_SIZE + NAME_SIZE + 2]);

    return rresult;
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
    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    
    char buf[BUF_SIZE], message[BUF_SIZE];

    struct  timeval tr;     // timeval_receive
    struct  timeval ts;     // timeval_send
    fd_set  readfds;        // CONTROLS SELECT

    // fd = fileno(stdin);

    int namelen;
    struct sockaddr_in serv_addr;

    int prompt_printed = 0;

    int cmdcode;
    char sender[NAME_SIZE];

    sock = socket(PF_INET, SOCK_STREAM, 0);   
    if (sock == -1)
        error_handling("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    tr.tv_sec  = RECV_TIMEOUT_SEC;
    tr.tv_usec = RECV_TIMEOUT_USEC;

    // SET TIMEOUT OF read()
    // https://stackoverflow.com/a/2939145
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof(tr));
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    
    printf("CONNECTED TO SERVER.\n\n");

    int warned = 0;

    // NICKNAME INITIALIZATION LOOP
    while (1)
    {
        printf("NICKNAME: ");

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
            sprintf(message, "2000");
            sprintf(&message[CMDCODE_SIZE + 1], "%s", buf);
            
            write(sock, message, CMDCODE_SIZE + 1 + namelen);

            memset(message, 0, BUF_SIZE);
            read(sock, message, 2);

            if (atoi(message) == 0) {
                moveCursorUp(warned ? 2 : 1, 0);
                printf("Sorry, but the name %s is already taken.\n", buf);
                if (!warned) warned = 1;
            }
            
            else {
                printf("Name accepted.\n");
                memcpy(nname, buf, NAME_SIZE);
                break;
            }
        }
    }
    
    for (int i = 1; i < PP_LINE_SPACE; i++) printf("\n");

    int is_init = 1;

    // MESSAGE COMMUNICATION LOOP
    while (1)
    {
        int cmdcode_prev = cmdcode;

        // RECEIVE MESSAGE
        if (recv_msg(&cmdcode, sender, message) < 0) {
            // printf("\nNo message.\n");
        }
        else {
            // Attempts to retain what the user was typing after msg receival...
            // fgets(buf, BUF_SIZE, stdin);
            // printf("%s\n", buf);
            // write(STDIN_FILENO, stdin, sizeof(stdin));

            if (cmdcode == 1000 || cmdcode == 3000) {
                // PRINT MSG AFTER REMOVING PREVIOUS LINES
                if (!is_init) moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);
                else is_init = 0;

                if (cmdcode == 1000) {
                    if (cmdcode_prev == 1000) moveCursorUp(1, 0);
                    printf("%s============ %s ============\n\n\n", cmdcode_prev == 1000 ? "" : "\n\n", message);
                }
                else printf("\n%s sent: %s\n", sender, message);

                prompt_printed = 0;
            }
        }

        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        if (!prompt_printed) {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\n");
            printf("Input message(q/Q to quit): \n");
            prompt_printed = 1;
        }

        fflush(0);

        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // fd 0 = stdin

        ts.tv_sec  = SEND_TIMEOUT_SEC;
        ts.tv_usec = SEND_TIMEOUT_USEC;
        
        // SEND MESSAGE
        // ON STDIN STREAM DATA EXISTENCE
        if (select(1, &readfds, NULL, NULL, &ts) > 0)
        {
            // INPUT
            fgets(buf, BUF_SIZE, stdin);
            buf[strlen(buf) - 1] = 0;
            if (!strcmp(buf, "q") || !strcmp(buf, "Q")) break;

            // SEND
            send_msg(3000, buf);

            // READ (CREATE OUTPUT FROM SERVER MESSAGE)
            recv_msg(&cmdcode, sender, message);
            
            moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + 1, 0);
            printf("\n%s sent: %s\n", sender, message);

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
