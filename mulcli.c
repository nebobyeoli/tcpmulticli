// From ECHO CLIENT
// + MULTICAST RECEIVE

// 컴파일 시 아래 복사해서 사용: 메인 소스 명시 후 추가 소스명 모두 입력
// gcc -o mulcli mulcli.c list/list.c list/list_node.c list/list_iterator.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

/* 이중 연결 리스트 api
 * https://github.com/clibs/list
 */
#include "list.h"

//// 나중에 필요하다 싶으면 아래 변수들 함수들 헤더랑 따로 만들어서 담을 것

#define BUF_SIZE        1024 * 2    // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define CMDCODE_SIZE    4           // cmdcode의 크기
#define CMD_SIZE        20          // cmdmode에서의 명령어 최대 크기
#define NAME_SIZE       30          // 닉네임 최대 길이

#define RECV_TIMEOUT_SEC    0
#define RECV_TIMEOUT_USEC   50000   // 1000000 usec = 1 sec
#define SEND_TIMEOUT_SEC    0
#define SEND_TIMEOUT_USEC   100000

#define MIN_ERASE_LINES     1       // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE       3       // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

// 지정된 멀티캐스팅 주소
char mulcast_addr[] = "239.0.100.1";

int sock;                   // 서버 소켓
char nname[NAME_SIZE];      // 자기 자신의 닉네임

// prompt-print message, 즉 '입력 문구'
char pp_message[] = "Input message(CTRL+C to quit):\r\n";

// command mode message, 즉 cmd mode에서의 입력 문구
char cmd_message[] = "Enter command(ESC to quit):\r\n> ";

// 함수명 변경: error_handling() >> perror_exit()
void perror_exit(char *message)
{
    printf("%s", message);
    exit(0);
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
 * \b       커서를 1칸 앞으로 옮긴다.
 *          goto 1 char b ack
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

// LIST DATA TO BUFFER
/* 인수들
 * 
 * char   *buf      : 리스트 데이터를 저장할 배열
 * list_t *list     : 읽을 리스트
 * int    *list_len : 리스트 길이 저장 변수
 * int     emptylist: 리스트 초기화 여부
 */
void transfer_list_data(char *buf, list_t *list, int *list_len, int emptylist)
{
    list_node_t *node;
    list_iterator_t *it = list_iterator_new(list, LIST_HEAD);

    int offset = 0;
    
    // 각 노드에 단일 char가 저장되어 있는 list의 입력 데이터를
    // buf 배열에 저장!
    while (node = list_iterator_next(it))
    {
        sprintf(&buf[offset++], "%c", node -> val);
        
        // buf 배열에 저장 완료된 node값은 free해 준다.
        if (emptylist) list_remove(list, node);
    }

    //// 리스트 내용 복사 완료 ////
    
    list_iterator_destroy(it);

    if (emptylist) *list_len = 0;
}

// https://stackoverflow.com/a/448982
/*
 * "기존 터미널 모드" 저장 변수입니다
 * struct termios를 이용해 입력 방식을 바꿀 수 있다고 합니다!
 * 요걸로 엔터 없는, 타이핑 중에서의 직접 입력을 구현할 수 있습니다! 와!!
 */
struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

/* COMMAND KEY들의 입력 가능을 위한, 기존 사용자 정의 getch()의 변형 사용!
 * 
 * 필요성:
 * 
 *   COMMAND KEY들, 예를 들어 방향키와 DELETE 키, ALT + KEY 같은 특수 키들의 경우에는
 *   일반 키와는 달리 한번에 3 ~ 5개의 키 인식 정수들을 날려 줍니다!
 * 
 *   Pull Request의 [other?] 단락에 작성한 표를 참고하십쇼!
 *   https://github.com/nebobyeoli/tcpmulticli/pull/4
 * 
 * 방법:
 * 
 *   일반적으로 char 단위 입력으로 알고 있는 입력 단위를
 *   char[] 단위로 바꾸어 줍니다!
 * 
 *   이렇게 하면 하나의 키를 눌렀을 때 반환되는 정수들의 값들과 총 개수로 판정해
 *   키 입력별로 COMMAND KEY 여부 및 종류를 구분할 수 있게 되어
 *   보다 광범위한 키 입력의 사용이 가능해집니다!
 *   특히 ESC 키와 방향키를 제대로 구분할 수 있게 됩니다!
 * 
 * YES, IT ALSO CONSUMES CTRL + C.
 * 즉 이제 프로그램 종료 시 뭐 좀 치울 거 밀고 나서 종료하도록 할 수 있게 되었습니다!
 */
int getch()
{
    // 어쩌다 kbhit()까지 확인했는데도 read()할 거 없으면 그냥 그거 반환하기로
    int r;

    // 키 하나 눌렀을 때 반환되는 정수들
    // 추가 키 사용할 때마다 그 키의 반환 개수에 따라 배열 길이 늘려서 사용할 수 있음!
    char c[5] = { 0, };

    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    }

    else
    {
        // COMMAND KEY DETECTED.
        if (c[0] == 27)
        {
            switch (strlen(c))
            {
                // ESC
                // 현재 알고 있는 바로는 유일하게 한 개의 정수, 27만을 반환하는 COMMAND KEY
                case 1:
                    return c[0];
                    break;

                // ALT + KEY
                // 원래 키의 음수값으로 사용하도록 지정했음
                case 2:
                    return -1 * c[1];
                
                // ARROWS
                // 방향키
                // 입력반환값: 27, '[', 'A/B/C/D'
                case 3:
                    return c[2] - 48;
                    break;
                
                case 4:
                    // DEL KEY
                    // '128'로 지정했음
                    if (c[0] == 27 && c[1] == 91 && c[2] == 51 && c[3] == 126) return 128;
                    break;

                // 더 쓸 거 있으면 추가할 것
                default:
                    break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage : %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }
    
    /* buf[]와 message[]의 용도 규칙은 따로 없으나
     * 보통 입력 저장 및 임시 저장소는 buf[],
     * NULL칸으로 분리된 형식의 수신/전송용 메시지는 message[]를 사용.
     * 필요, 흐름, 문맥에 따라 둘의 용도를 '배열1', '배열2'와 같이 교대로 사용하기도 함
     */
    char buf[BUF_SIZE];
    char message[BUF_SIZE];
    char cmd[CMD_SIZE];

    /* 실제 입력 관리: 이중 리스트 api 사용.
     * 엔터를 쳤을 때 blist 또는 clist의 데이터를 buf[]로 저장하고,
     * 해당 리스트를 초기화한다.
     * 
     * bllen, cllen은 각 지정 리스트에서의 노드(입력된 char)의 수를 의미한다.
     * 길이 확인용 변수로 실제 삽입/삭제 작업에는 쓰이지 않는다. (!= list_iterator_t *it)
     */
    list_t *blist = list_new();     // buf_list
    int bllen = 0;                  // buf_list_len

    list_t *clist = list_new();     // cmd_list
    int cllen = 0;                  // cmd_list_len

    struct  timeval tr;     // timeval_receive
    fd_set  readfds;        // CONTROLS SELECT

    int namelen;                    // 자기 자신의 닉네임 길이 (닉네임 설정 때 사용)
    struct ip_mreq join_addr;       // 멀티캐스트 주소 저장
    struct sockaddr_in serv_addr;   // 서버 주소 저장
    
    /* 필요할 때만 '입력 문구'를 출력하도록 함.
     * 코드 진행 이후 '입력 문구'의 출력이 필요할 때는 prompt_printed = 1 실행
     */
    int prompt_printed = 0;
    
    /* COMMAND MODE
     * ESC 눌러서 실행
     */
    int cmdmode = 0;

    int cmdcode;
    char sender[NAME_SIZE];

    sock = socket(PF_INET, SOCK_STREAM, 0);   
    if (sock == -1) perror_exit("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    tr.tv_sec  = RECV_TIMEOUT_SEC;
    tr.tv_usec = RECV_TIMEOUT_USEC;
    
    // JOIN MULTICAST GROUP
    // 주어진 멀티캐스팅 주소로 연결한다.
    join_addr.imr_multiaddr.s_addr = inet_addr(mulcast_addr);
    join_addr.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&join_addr, sizeof(join_addr));

    // SET TIMEOUT OF read()
    // https://stackoverflow.com/a/2939145
    // 이렇게 하면 아예 소켓 옵션으로 read()에 타임아웃을 걸 수 있어 유용하다.
    // 바로 위 setsockopt()과는 별개임!
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof(tr));
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        perror_exit("connect() error!");
    
    printf("CONNECTED TO SERVER.\n\n");

    /* prompt_printed와 비슷한 역할인데,
     * 입력 조건 메시지가 처음 출력되는 때와 그 다음부터 출력되는 때를 구분짓는다.
     *
     * 닉네임 조건 불충족 문구 출력 위치를 동일 위치로 고정해 주기 위해 존재한다.
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

        else if (buf[0] == ' ')
        {
            moveCursorUp(warned ? 2 : 1, 0);
            printf("Name must not start with a SPACE character...\n");
            if (!warned) warned = 1;
        }
        
        else if (namelen > NAME_SIZE)
        {
            moveCursorUp(warned ? 2 : 1, 0);
            printf("Name must be shorter than %d characters!\n", NAME_SIZE);
            if (!warned) warned = 1;
        }

        else
        {
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

    fflush(0);

    ////// MESSAGE COMMUNICATION LOOP //////
    
    //// 이때부터 글자 하나씩 입력받기 모드 시작.
    set_conio_terminal_mode();

    memset(buf, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    memset(cmd, 0, CMD_SIZE);

    // int bi = 0;     // buf string index
    // int ci = 0;     // cmd string index

    while (1)
    {
        // 서버
        // 복사하자 -

        // 이전 cmdcode값 저장
        int cmdcode_prev = cmdcode;

        // RECEIVE MESSAGE
        // recv_msg()에서 read()를 실행하여 setsockopt으로 설정한 대기 시간만큼 기다린다.
        if (recv_msg(&cmdcode, sender, message) < 0) {
            // printf("No message.\r\n");
        }
        
        else
        {
            if (cmdcode == 1500) {
                write(sock, "1500", CMDCODE_SIZE);
            }

            if (cmdcode == 1000 || cmdcode == 3000) {
                // PRINT MSG AFTER REMOVING PREVIOUS LINES
                if (!is_init) moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1);
                else is_init = 0;

                // CODE 1000: MESSAGE FROM SERVER
                if (cmdcode == 1000)
                {
                    // 이전 메시지도 서버 메시지일 경우 줄넘김 간격 하나 줄여줌
                    // 즉 클라이언트에서 서버로(그 반대도 포함) 전송자가 바뀐 경우에만 줄넘김 좀 더 넓혀 줌
                    if (cmdcode_prev == 1000) moveCursorUp(1, 0);
                    printf("%s============ %s ============\r\n\r\n\r\n", cmdcode_prev == 1000 ? "" : "\r\n\r\n\r\n", message);
                }

                // CODE 3000: MESSAGE FROM CLIENT
                else printf("\r\n%s sent: %s\r\n", sender, message);

                prompt_printed = 0;
            }
        }

        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        // 수신 메시지 존재 or 입력 버퍼 비워짐으로 추가 출력이 존재하는 경우에만 '입력 문구' 출력.
        if (!prompt_printed)
        {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");
            printf("%s", cmdmode ? cmd_message : pp_message);
            prompt_printed = 1;
            fflush(stdout);
        }

        fflush(0);

        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // fd 0 = stdin

        // SEND MESSAGE
        // ON STDIN STREAM DATA EXISTENCE
        
        // kbhit() 내에서 select() 돌아감, 추가적 select() 필요 없음
        // 그래서 timeval ts 지움
        if (kbhit())
        {
            int c = getch();
            
            // cmdmode 여부에 따라 다른 리스트와 버퍼 배열 사용.
            /* message : 입력 리스트 blist, 리스트 노드 수 bllen, 버퍼 배열 buf 
             * cmdmode : 입력 리스트 clist, 리스트 노드 수 cllen, 버퍼 배열 cmd
             */
            switch (c)
            {
                // note: TO-DOS
                // mulfd에서의 키 입력 방식에 맞춰 switch(c) 내부 내용
                // 갱신해 주어야 함
                // <...>

                // PRESSED CTRL + C
                // 99 & 037
                case 3:
                {
                    reset_terminal_mode();

                    close(sock);

                    moveCursorUp(PP_LINE_SPACE, 0);
                    printf("\r\nClosed client.");
                    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");

                    return 0;
                }

                // PRESSED ESC
                case 27:
                {
                    if (cmdmode)
                    {
                        // cmdmode에서 나갈 때
                        memset(cmd, 0, CMD_SIZE);
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1);
                        cmdmode = 0;
                    }

                    else
                    {
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);
                        cmdmode = 1;
                    }

                    prompt_printed = 0;
                    break;
                }

                // 방향키까지 고려하게 되면 리스트 구조체 써야 함
                // 뭐 나중에 쓸수도 있지만
                // 쓰는 게 나을 수도 있겠지만.
                // 아마도 나중엔 쓰는 게 나을 듯.
                // 
                // 쓸거임
                case 17: case 18: case 19: case 20:
                {
                    // UP, DOWN, RIGHT, LEFT
                    // please don't enable the arrow keys  yet

                    break;
                }

                // PRESSED BACKSPACE
                case 127:
                {
                    // cmdmode에서는 줄넘김 안 쓰는 걸로 가정.
                    if (cmdmode && cllen > 0)
                    {
                        printf("\b \b");
                        list_rpop(clist);
                        cllen--;
                    }

                    // default (message) mode
                    else if (bllen > 0)
                    {
                        if (blist -> tail -> val == '\n')
                        {
                            printf("\033[A");
                            list_rpop(blist);
                            list_rpop(blist);
                            bllen -= 2;

                            // move cursor right
                            // iterate through list
                            list_node_t *node = blist -> tail;
                            while (node -> val != '\n') {
                                printf("\033[C");
                                if (node == blist -> head) break;
                                node = node -> prev;
                            }
                        }
                        
                        else
                        {
                            printf("\b \b");
                            list_rpop(blist);
                            bllen--;
                        }
                    }
                    
                    break;
                }

                // PRESSED CTRL + BACKSPACE
                case 8:
                {
                    if (cmdmode)
                    {
                        /* 줄넘김과 두 번째 공백 전까지 지움
                         * 두 번째 공백: e.g. 맨 끝 글자가 공백 문자인 상태에서 CTRL + BACKSPACE를 누른 경우
                         * 그 공백을 지움과 함께 그 다음 공백까지 지움
                         * 
                         * 반복문 세 개: 각각 그 첫 번째 공백까지, 공백이 없는 구간, 두 번째 공백까지의
                         * 문자 지움 지시를 의미함
                         */

                        for (cllen; cllen > 0 && clist -> tail -> val != '\n' && clist -> tail -> val == ' '; printf("\b \b"), list_rpop(clist), cllen--);
                        for (cllen; cllen > 0 && clist -> tail -> val != '\n' && clist -> tail -> val != ' '; printf("\b \b"), list_rpop(clist), cllen--);
                        for (cllen; cllen > 0 && clist -> tail -> val != '\n' && clist -> tail -> val == ' '; printf("\b \b"), list_rpop(clist), cllen--);
                    }

                    else
                    {
                        for (bllen; bllen > 0 && blist -> tail -> val != '\n' && blist -> tail -> val == ' '; printf("\b \b"), list_rpop(blist), bllen--);
                        for (bllen; bllen > 0 && blist -> tail -> val != '\n' && blist -> tail -> val != ' '; printf("\b \b"), list_rpop(blist), bllen--);
                        for (bllen; bllen > 0 && blist -> tail -> val != '\n' && blist -> tail -> val == ' '; printf("\b \b"), list_rpop(blist), bllen--);
                    }
                    
                    break;
                }

                // PRESSED DELETE
                case 128:
                {
                    // 
                    // 방향키 쓰면 쓸거임
                    // 
                    break;
                }

                // PRESSED ENTER
                case 13:
                {
                    // 내용 없이 엔터 때린 경우는 무시
                    if (bllen == 0) break;

                    if (cmdmode)
                    {
                        transfer_list_data(cmd, clist, &cllen, 1);

                        // 
                        // DO SOMETHING WITH THE COMMAND HERE...
                        // 명령어에 따른 동작 실행 e.g. 클라이언트 목록 뽑기
                        // cmd[some_index] ...
                        // 
                    }

                    else
                    {
                        transfer_list_data(buf, blist, &bllen, 1);

                        // SEND
                        send_msg(3000, buf);

                        // READ (CREATE OUTPUT FROM SERVER MESSAGE)
                        recv_msg(&cmdcode, sender, message);
                        
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1);
                        printf("\r\n%s sent: %s\r\n", sender, message);

                        prompt_printed = 0;

                        memset(buf, 0, BUF_SIZE);
                    }

                    break;
                }

                // PRESSED ALT + ENTER
                // 줄넘김 삽입.
                case -13:
                {
                    if (cmdmode)
                    {
                        // cmdmode에서 줄넘김 삽입 쓸 게 있을까?
                    }

                    else
                    {
                        printf("\r\n");
                        list_rpush(blist, list_node_new('\r'));
                        list_rpush(blist, list_node_new('\n'));
                        bllen += 2;
                    }

                    break;
                }
                
                // PRESSED OTHER
                // 타이핑 중
                default:
                {
                    printf("%c", c);

                    if (cmdmode)
                    {
                        list_rpush(clist, list_node_new(c));
                        cllen++;
                    }
                    
                    else
                    {
                        list_rpush(blist, list_node_new(c));
                        bllen++;
                    }

                    break;
                }
            }

            fflush(stdout);
        }
    }
}
