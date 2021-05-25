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

#define BUF_SIZE        1024 * 4    // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define CMDCODE_SIZE    4
#define NAME_SIZE       30
#define MAX_SOCKS       100

#define GETSERVMSG_TIMEOUT_SEC  0
#define GETSERVMSG_TIMEOUT_USEC 50000   // 1000000 usec = 1 sec

#define MIN_ERASE_LINES     1   // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE       3   // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

char mulcast_addr[] = "239.0.100.1";

int client[MAX_SOCKS];
char names[MAX_SOCKS][NAME_SIZE];

// prompt-print message, 즉 '입력 문구'
char pp_message[] = "Input message(q/Q to quit): \n";

// 'emoji' 문자열 임시 내장 - 파일입출력으로의 사용 전까지
char myh[] = "⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀\n⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀\n⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀\n⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀\n⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀\n⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀\n⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀\n⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆\n⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦\n⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷\n⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹\n⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿\n⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷";

char bigbird[] = "                         .,,;!!=:-                \n                      ,-*@#@@@@@@$~               \n                     *$@@@@@@@@@@@@@~             \n                   !#@@@@@@@@@@@@@@@@@*.          \n                 -*#@@@@@@@@@@@@@@@@@@@=,        ~\n                .;@@!:#@@@@@@@@@@@@@@@@@#:   . ,#:\n               ,=@@=~,;@@@@@@@@@@@@@@@@@@@@@@@@@@.\n               @@@#~-- @#@@@@@@@@@@@@@@@@@@@@@@@= \n             .!@@@#::- =:=@@@@@@@@@@@@@@@@@@@@@@. \n             !@##-$*-- #::@@@@@@@@@@@@@@@@@@@@@$  \n             #-:@~*#, .#::~#@@@@@@@@@@@@@@@@@@@$  \n            :#: #*.-$==@$#!,@@@@@@@@@@@@@@@@@@@.  \n            $$~ *,. -=;.#@@@@@@@@@@@@@@@@@@@@@@   \n           .@#.,=-~; $;!#*,#@@@@@@@@@@@@@@@@@@;   \n           ,@@##$-;~ $-==!$.@@@@@@@@@@@@@@@@@@,   \n           ~@@$~!~-* #:.$!* #@@@@@@@@@@@@@@@@@,   \n           :@@#:::: !#$$;!-~##@@@@@@@@@@@@@@@@    \n           ~@@$,;@=$#;;!: $#~.$@@@@@@@@@@@@@@@.   \n           -@###*...@- ;*,-@-!-#@@@@@@@@@@@@@@,   \n           ,$~~$;,: ~##,*-.#!:-@@@@@@@@@@@@@@@    \n           ,#~~~;$$; ##$  :#@;#@@@@#@@@@@@@@@$    \n            @;!:*~~. !##=*#:-*@@@@@@@@@@@@@@@;    \n            #=~$!~ !-~@###!~~.#@@@@@@@@@@@@@@-    \n            !@-**;.=; $, #;=; $@@@@@@@@@@@@@#.    \n           -##~,$=;~; #;-**;; =@@@@@@@@@@@@@*     \n          =@@@;:#::., @;*!$-. $#@@@@@@@@@@@@.     \n        .#@@@@#$@;~,,,#--!@*;$@@@@@@@@@@@@@:      \n       ;@#@@@@@@@#~::$#@$@##@#@@@@@@@@@@@@$       \n     .###@@@@@@@@@@#@@#@@@@@@@@@@@@@@@@@@@;       \n    :@@@@*!;--,!@@@@@@##@@@@@@@@@@@@@@@@@*        \n  .:-           ,#@@@@@@@@@@@@@@@@@@@@@@:         \n          ,::$,!. $@@@@@@@@@@@@@@@@@@@$           \n         ;##@@@@@= *#@@@@@@@@@@@@@@@@-            \n        -$~=$#@@@@# .@@#@@@@@@@@@@@@@@-           \n        .!;;*$##@@@#=@: -:*@@@@$~.,*@@@;          \n         :,!,;:!,.@@@;      #@@@.   -#@@#~        \n         :.-       !         =@@@=    $@@*        \n         ;;-,                 *@@;     @@:        \n        ,.,~                 .@@#     -@@         \n        .. ,                 #@@      *@!         \n        ,. .         .      #@@       #@          \n        -  ..    ,@=## .   #@@.  -   -@@          \n        -  ,.   ,#$@@@=#  #@#.*!!@-- =@-          \n        ....   ,=~$#-=@@$@@#.,@@@@@@;@#           \n               - =* ~*.!;;;  #=#$.@@@@~           \n                !. .!       ::,# ;~               \n               .   :        - =  !                \n                             :  -                 \n                                .\n";

void perror_exit(char *message)
{
    perror(message);
    exit(0);
}

// SEND TO ALL CLIENTS
/*
 * char *servlog는 "MESSAGE FROM SERVER: %s" 형식으로
 * 서버 실행 터미널에 출력될 메시지를 말한다.
 *
 * 서버에서 직접 작성된 메시지를 전송할 때는 그냥 char *msg에와 같은 값을 넣어 주고,
 * 클라이언트 메시지 전달과 같이 "메시지 알림"이 필요 없을 때는 그냥 NULL을 넣어주면 된다.
 */
void sendAll(int clnt_cnt, int cmdcode, char *sender, char *msg, char *servlog)
{
    int i;
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    
    /* sprintf()를 이용해,
     * 한 개의 NULL 문자를 사이에 두고 char 배열에 작성하는 기법으로
     * 각 메시지 데이터를 구분지어 저장한다.
     */
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
        // 서버에서 나갔거나, 닉네임이 설정되지 않은 클라이언트는 무시한다.
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], message, BUF_SIZE);
        printf("Sent to client [%d] (%s)\n", client[i], names[i]);
    }
}

// NOTE. this is for LINUX
// https://stackoverflow.com/a/35190285
/* 
 * \33[2K   현재 커서가 있는 줄을 모두 지운다.
 *          erases the entire line your cursor is currently on
 *
 * \033[A   커서를 한 줄 위로 올린다. 단, 줄 내에서의 커서 위치는 변하지 않는다.
 *          moves your cursor up one line, but in the same column
 *          i.e. not to the start of the line
 * 
 * \r       커서를 현재 줄의 맨 앞으로 옮긴다.
 *          brings your cursor to the beginning of the line
 *          (r is for carriage return, N.B. carriage returns
 *           do not include a newline so cursor remains on the same line)
 *          but does not erase anything
 * 
 * \b       1칸 지운다.
 *          erase 1 char b ack
 */
void moveCursorUp(int lines, int eraselast)
{
    // eraselast가 설정된 경우, 커서를 윗줄로 올리기 전에 커서가 있던 줄을 지우고 간다.
    if (eraselast) printf("\33[2K");

    // 커서 위 lines개의 줄을 지운다.
    for (int i = 0; i < lines; i++) printf("\033[A\33[2K");

    // \033[A는 커서를 처음으로 옮겨 주지 않으므로, 커서를 맨 앞으로 옮겨 준다.
    printf("\r");

    // 출력 버퍼 비움
    fflush(stdout);
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

    // int len;
    
    index = strstr(message, ":myh:");
    if (index)
    {
        inbet = (int)index - (int)message;
        // len = (int)strlen(mdest);
        
        memcpy(mdest, message, inbet);
        // /* way 1 */ sprintf(&mdest[len + inbet], "\n%s\n", myh);
        sprintf(&mdest[inbet], "\n%s\n%s", myh, &message[inbet + sizeof(":myh:") - 1]);
    }

    printf("%s\n", message);
    printf("%s\n", mdest);

    // 
    // message and mdest role swap time!
    // 

    index = strstr(mdest, ":bigbird:");
    
    if (index)
    {
        inbet = (int)index - (int)mdest;
        // len = (int)strlen(message);

        memcpy(message, mdest, inbet);
        sprintf(&message[inbet], "\n%s\n%s", bigbird, &mdest[inbet + sizeof(":bigbird:") - 1]);
    }

    memcpy(mdest, message, BUF_SIZE);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    int serv_sock, clnt_sock, state;
    struct ip_mreq join_addr;
    struct sockaddr_in clnt_addr, serv_addr;

    struct timeval ti;          // timeval_(std)input - 입력값에 대한 타임아웃 구조체
    fd_set stdinfd;             // 입력값 존재에 대한 select 타이머 사용 용도
    fd_set readfds, allfds;     // 클라이언트 배열 순회 용도, 둘의 정확한 차이나 용도는 불명
    
    /* buf[]와 message[]의 용도 규칙은 따로 없으나
     * 보통 입력 저장 및 임시 저장소는 buf[],
     * NULL칸으로 분리된 형식의 수신/전송용 메시지는 message[]를 사용.
     * 필요, 흐름, 문맥에 따라 둘의 용도를 '배열1', '배열2'와 같이 교대로 사용하기도 함
     */
    char buf[BUF_SIZE], message[BUF_SIZE];

    /* sendAll()의 사용 형식에 맞추기 위한 '서버 이름'.
     * sendAll()에 사용할 때는 char *sender 인자에 넣으면 됨
     */
    char serv_name[NAME_SIZE] = "SERVER";
    
    int i, j, clnt_size, clnt_cnt, fd_max;

    /* 필요할 때만 '입력 문구'를 출력하도록 함.
     * 코드 진행 이후 '입력 문구'의 출력이 필요할 때는 prompt_printed = 1 실행
     */
    int prompt_printed = 0;

    state = 0;

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) perror_exit("socket() error");

    memset(&serv_addr, 0x00, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    
    // JOIN MULTICAST GROUP
    // 239.0.100.1로 조인한다.
    join_addr.imr_multiaddr.s_addr = inet_addr(mulcast_addr);
    join_addr.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(serv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&join_addr, sizeof(join_addr));

    state = bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (state == -1) perror_exit("bind() error");

    state = listen(serv_sock, 5);
    if (state == -1) perror_exit("listen() error");

    clnt_sock = serv_sock;

    ////// INITIALIZE //////

    /* 원본 iomux_select.c에서 clnt_cnt 조건 수정: 실제 개수로써 사용되도록.
     * 아래 클라이언트 반복문에서도 값 설정 때 i가 아닌 i + 1과 비교하고 대입하여 사용
     */
    clnt_cnt = -1;

    fd_max = serv_sock;

    for (i = 0; i < MAX_SOCKS; i++) client[i] = -1;

    FD_ZERO(&readfds);
    FD_SET(serv_sock, &readfds);

    printf("\nStarted TCP Server.\n");
    fflush(stdout);

    ////// CLIENT INTERACTION LOOP. //////

    while (1)
    {
        FD_ZERO(&stdinfd);
        FD_SET(0, &stdinfd);  // fd 0 = stdin

        ti.tv_sec  = GETSERVMSG_TIMEOUT_SEC;
        ti.tv_usec = GETSERVMSG_TIMEOUT_USEC;
        
        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        // 수신 메시지 존재 or 입력 버퍼 비워짐으로 추가 출력이 존재하는 경우에만 '입력 문구' 출력.
        if (!prompt_printed) {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\n");
            printf("%s", pp_message);
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
            // 아래에 대한 구체적 주석은 emojis 브랜치의 check_append_emojis()에 작성할 것.
            // 아래 내용은 해당 함수의 사용으로 대체될 것.
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
                    // 연결 해제된 클라이언트의 이름을 지워 준다
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

                    //// MODE : SET NAME ////
                    
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

                    //// MODE : MESSAGE ////

                    else if (cmdcode == 3000)
                    {
                        // NEW BUFFER FOR USAGE AS PARAMETER FOR sendAll()
                        char msg[BUF_SIZE];
                        // EXTRACT MESSAGE FROM RECV BUFFER
                        strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                        
                        // CHECK FOR EMOJIS
                        // 아래에 대한 구체적 주석은 emojis 브랜치의 check_append_emojis()에 작성할 것.
                        // 아래 내용은 해당 함수의 사용으로 대체될 것.
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
