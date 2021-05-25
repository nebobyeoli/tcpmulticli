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
#define RECV_TIMEOUT_USEC   50000  // 1000000 usec = 1 sec
#define SEND_TIMEOUT_SEC    0
#define SEND_TIMEOUT_USEC   100000

#define MIN_ERASE_LINES     1   // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE       2   // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

char mulcast_addr[] = "239.0.100.1";

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
/* 
 * int *cmdcode: cmdcode값을 직접 수정해 주기 때문에 포인터로 받는다.
 * 각 크기 변수를 배열 인덱스로 활용해, 메시지 데이터들을 인자로 주어진 각 변수에 저장한다.
 */
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
    struct ip_mreq join_addr;
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
    
    // JOIN MULTICAST GROUP
    // 239.0.100.1로 조인한다.
    join_addr.imr_multiaddr.s_addr = inet_addr(mulcast_addr);
    join_addr.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&join_addr, sizeof(join_addr));

    // SET TIMEOUT OF read()
    // https://stackoverflow.com/a/2939145
    // 이렇게 하면 아예 소켓 옵션으로 read()에 타임아웃을 걸 수 있어 유용하다.
    // 바로 위 setsockopt()과는 별개임!
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof(tr));
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    
    printf("CONNECTED TO SERVER.\n\n");

    /* prompt_printed와 비슷한 역할인데,
     * 입력 조건 메시지가 처음 출력되는 때와 그 다음부터 출력되는 때를 구분짓는다.
     *
     * 문구 출력 위치를 동일 위치로 고정해 주기 위해 존재한다.
     */
    int warned = 0;

    ////// NICKNAME INITIALIZATION LOOP //////

    while (1)
    {
        printf("NICKNAME: ");

        fgets(buf, BUF_SIZE, stdin);

        // 마지막 줄넘김 없애기: '\n' 문자까지 대기 후 (int)0으로 대체
        for (namelen = 0; buf[namelen] != '\n'; namelen++);
        buf[namelen] = 0;
        
        // 그냥 엔터만 넘긴 경우는 무시한다.
        if (namelen < 1) moveCursorUp(1, 0);

        //// 입력받은 버퍼에서 닉네임 조건 충족 여부를 먼저 확인한 후에 서버로 넘긴다.

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

    ////// MESSAGE COMMUNICATION LOOP //////
    
    while (1)
    {
        int cmdcode_prev = cmdcode;

        // RECEIVE MESSAGE
        // recv_msg()에서 read()를 실행하여 setsockopt으로 설정한 대기 시간만큼 기다린다.
        if (recv_msg(&cmdcode, sender, message) < 0) {
            // printf("No message.\n");
        }
        
        else {
            // 주석코드는 입력 버퍼 유지에 대한 시도이니 무시해도 됨
            // 해결법 있으면 알려주세요
            // Some attempts to retain what the user was typing after msg receival...
            // fgets(buf, BUF_SIZE, stdin);
            // printf("%s\n", buf);
            // write(STDIN_FILENO, stdin, sizeof(stdin));

            if (cmdcode == 1000 || cmdcode == 3000) {
                // PRINT MSG AFTER REMOVING PREVIOUS LINES
                if (!is_init) moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);
                else is_init = 0;

                // CODE 1000: MESSAGE FROM SERVER
                if (cmdcode == 1000) {
                    if (cmdcode_prev == 1000) moveCursorUp(1, 0);
                    printf("%s============ %s ============\n\n\n", cmdcode_prev == 1000 ? "" : "\n\n", message);
                }

                // CODE 3000: MESSAGE FROM CLIENT
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
        }

        fflush(0);
    }
    
    close(sock);
    return 0;
}
