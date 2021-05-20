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

#define BUF_SIZE        1024 * 4
#define CMDCODE_SIZE    4
#define NAME_SIZE       30
#define MAX_SOCKS       100

#define GETSERVMSG_TIMEOUT_SEC  0
#define GETSERVMSG_TIMEOUT_USEC 50000  // 1000000 usec = 1 sec

#define MIN_ERASE_LINES     1
#define PP_LINE_SPACE       3       // min: 1  // prompt print ('Input message~') line space

int client[MAX_SOCKS];
char names[MAX_SOCKS][NAME_SIZE];

char myh[] = "⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀\n⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀\n⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀\n⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀\n⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀\n⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀\n⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀\n⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆\n⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦\n⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷\n⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹\n⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿\n⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷";

char bigbird[] = "                         .,,;!!=:-                \n                      ,-*@#@@@@@@$~               \n                     *$@@@@@@@@@@@@@~             \n                   !#@@@@@@@@@@@@@@@@@*.          \n                 -*#@@@@@@@@@@@@@@@@@@@=,        ~\n                .;@@!:#@@@@@@@@@@@@@@@@@#:   . ,#:\n               ,=@@=~,;@@@@@@@@@@@@@@@@@@@@@@@@@@.\n               @@@#~-- @#@@@@@@@@@@@@@@@@@@@@@@@= \n             .!@@@#::- =:=@@@@@@@@@@@@@@@@@@@@@@. \n             !@##-$*-- #::@@@@@@@@@@@@@@@@@@@@@$  \n             #-:@~*#, .#::~#@@@@@@@@@@@@@@@@@@@$  \n            :#: #*.-$==@$#!,@@@@@@@@@@@@@@@@@@@.  \n            $$~ *,. -=;.#@@@@@@@@@@@@@@@@@@@@@@   \n           .@#.,=-~; $;!#*,#@@@@@@@@@@@@@@@@@@;   \n           ,@@##$-;~ $-==!$.@@@@@@@@@@@@@@@@@@,   \n           ~@@$~!~-* #:.$!* #@@@@@@@@@@@@@@@@@,   \n           :@@#:::: !#$$;!-~##@@@@@@@@@@@@@@@@    \n           ~@@$,;@=$#;;!: $#~.$@@@@@@@@@@@@@@@.   \n           -@###*...@- ;*,-@-!-#@@@@@@@@@@@@@@,   \n           ,$~~$;,: ~##,*-.#!:-@@@@@@@@@@@@@@@    \n           ,#~~~;$$; ##$  :#@;#@@@@#@@@@@@@@@$    \n            @;!:*~~. !##=*#:-*@@@@@@@@@@@@@@@;    \n            #=~$!~ !-~@###!~~.#@@@@@@@@@@@@@@-    \n            !@-**;.=; $, #;=; $@@@@@@@@@@@@@#.    \n           -##~,$=;~; #;-**;; =@@@@@@@@@@@@@*     \n          =@@@;:#::., @;*!$-. $#@@@@@@@@@@@@.     \n        .#@@@@#$@;~,,,#--!@*;$@@@@@@@@@@@@@:      \n       ;@#@@@@@@@#~::$#@$@##@#@@@@@@@@@@@@$       \n     .###@@@@@@@@@@#@@#@@@@@@@@@@@@@@@@@@@;       \n    :@@@@*!;--,!@@@@@@##@@@@@@@@@@@@@@@@@*        \n  .:-           ,#@@@@@@@@@@@@@@@@@@@@@@:         \n          ,::$,!. $@@@@@@@@@@@@@@@@@@@$           \n         ;##@@@@@= *#@@@@@@@@@@@@@@@@-            \n        -$~=$#@@@@# .@@#@@@@@@@@@@@@@@-           \n        .!;;*$##@@@#=@: -:*@@@@$~.,*@@@;          \n         :,!,;:!,.@@@;      #@@@.   -#@@#~        \n         :.-       !         =@@@=    $@@*        \n         ;;-,                 *@@;     @@:        \n        ,.,~                 .@@#     -@@         \n        .. ,                 #@@      *@!         \n        ,. .         .      #@@       #@          \n        -  ..    ,@=## .   #@@.  -   -@@          \n        -  ,.   ,#$@@@=#  #@#.*!!@-- =@-          \n        ....   ,=~$#-=@@$@@#.,@@@@@@;@#           \n               - =* ~*.!;;;  #=#$.@@@@~           \n                !. .!       ::,# ;~               \n               .   :        - =  !                \n                             :  -                 \n                                .\n";

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

    printf("Total msgsize: %d of %d maximum\n", CMDCODE_SIZE + NAME_SIZE + (int)strlen(msg), BUF_SIZE);

    for (i = 0; i < clnt_cnt; i++)
    {
        // DISCARD DISCONNECTED OR NAMELESS CLIENTS
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], message, BUF_SIZE);
        printf("Sent to client [%d] (%s)\n", client[i], names[i]);
    }
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

// EMBEDS EMOJIS TO MSG, REPLACING :[emojicommand]:
// CURRENTLY: CAN ONLY USE ONE EMOJI PER EMOJI TYPE
void check_append_emojis(char *msg, char *mdest)
{
    // EMOJI EMBEDDER
    // CURRENTLY SUPPORTS: EMBEDDED :myh: AND :bigbird:

    char message[BUF_SIZE];
    memcpy(message, msg, BUF_SIZE);
    memset(mdest, 0, BUF_SIZE);

    // TYPE *CHAR BECAUSE OF MSG TYPE,
    // BUT USED INT-CASTED FOR GETTING SIZE USING MEMORY LOCATION COMPARISONS
    char *index;

    // CAST TO INT FOR MEMORY LOCATION INDICATION
    // GET THE SIZE VALUE INBETWEEN
    int inbet;

    int len;
    
    index = strstr(message, ":myh:");
    if (index)
    {
        inbet = (int)index - (int)message;
        len = (int)strlen(mdest);
        
        memcpy(&mdest[len], message, inbet);
        sprintf(&mdest[len + inbet], "\n%s\n", myh);
        // /* was previously */ sprintf(&mdest[0 + inbet], "\n%s\n%s", myh, &message[inbet + sizeof(":myh:") - 1]);
    }

    printf("%s\n", message);
    memcpy(message, &message[inbet + sizeof(":myh:") - 1], BUF_SIZE - (inbet + sizeof(":myh:")));
    printf("%s\n", message);

    index = strstr(message, ":bigbird:");
    
    if (index)
    {
        inbet = (int)index - (int)message;
        len = (int)strlen(mdest);

        memcpy(&mdest[len], message, inbet);
        sprintf(&mdest[len + inbet], "\n%s\n%s", bigbird, &message[inbet + sizeof(":bigbird:") - 1]);
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

    struct timeval ti;  // timeval_(std)input
    fd_set stdinfd;
    fd_set readfds, allfds;
    
    char buf[BUF_SIZE], message[BUF_SIZE];
    char serv_name[NAME_SIZE] = "SERVER";
    int i, j, clnt_size, clnt_cnt, fd_max;

    int prompt_printed = 0;

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

    printf("\nStarted TCP Server.\n");
    fflush(stdout);

    while (1)
    {
        FD_ZERO(&stdinfd);
        FD_SET(0, &stdinfd);  // fd 0 = stdin

        ti.tv_sec  = GETSERVMSG_TIMEOUT_SEC;
        ti.tv_usec = GETSERVMSG_TIMEOUT_USEC;
        
        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        if (!prompt_printed) {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\n");
            printf("Input message(q/Q to quit): \n");
            prompt_printed = 1;
        }

        // GET SERVER MSG
        // ON STDIN STREAM DATA EXISTENCE
        if (select(1, &stdinfd, NULL, NULL, &ti) > 0)
        {
            // INPUT
            memset(buf, 0, BUF_SIZE);
            fgets(buf, BUF_SIZE, stdin);
            buf[strlen(buf) - 1] = 0;
            if (!strcmp(buf, "q") || !strcmp(buf, "Q")) break;

            // if ()
            moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);

            // CHECK FOR EMOJIS
            char mdest[BUF_SIZE], umdest[BUF_SIZE] = "\nMESSAGE FROM SERVER:\n";
            check_append_emojis(buf, mdest);
            strcat(umdest, mdest);

            // moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);

            // SEND
            if (mdest[0])   sendAll(clnt_cnt, 1000, serv_name, umdest, mdest);
            else            sendAll(clnt_cnt, 1000, serv_name, buf, buf);

            prompt_printed = 0;
        }
        else
        {
            // TIMEOUT
            // printf("Timeout.\n");
        }

        ti.tv_sec  = GETSERVMSG_TIMEOUT_SEC;
        ti.tv_usec = GETSERVMSG_TIMEOUT_USEC;
        
        allfds = readfds;
        state = select(fd_max + 1, &allfds, 0, 0, &ti);

        // ACCEPT CLIENT TO SERVER SOCK
        if (FD_ISSET(serv_sock, &allfds))
        {
            clnt_size = sizeof(clnt_addr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_size);
            
            moveCursorUp(MIN_ERASE_LINES, 0);
            printf("Connection from (%s, %d)\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
            prompt_printed = 0;

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
                    moveCursorUp(MIN_ERASE_LINES, 0);
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

                    prompt_printed = 0;
                }

                else
                {
                    int cmdcode = atoi(buf);

                    moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);
                    printf("\nReceived from [%d] (%s): %s %s\n", client[i], names[i], buf, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                    prompt_printed = 0;

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
                        // NEW BUFFER FOR USAGE AS PARAMETER FOR sendAll()
                        char msg[BUF_SIZE];
                        // EXTRACT MESSAGE FROM RECV BUFFER
                        strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                        
                        // CHECK FOR EMOJIS
                        char mdest[BUF_SIZE], umdest[BUF_SIZE] = "\nMESSAGE FROM SERVER:\n";
                        check_append_emojis(msg, mdest);
                        strcat(umdest, mdest);

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);

                        // SEND
                        if (mdest[0]) sendAll(clnt_cnt, 3000, names[i], mdest, mdest);
                        else          sendAll(clnt_cnt, 3000, names[i], msg, msg);
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
