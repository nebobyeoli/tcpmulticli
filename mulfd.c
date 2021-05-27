// From TCP SERVER
// CAN HANDLE MULTIPLE CLIENTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

//// 나중에 필요하다 싶으면 아래 변수들 함수들 헤더랑 따로 만들어서 담을 것

#define BUF_SIZE        1024 * 2    // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define CMDCODE_SIZE    4           // cmdcode의 크기
#define CMD_SIZE        20          // cmdmode에서의 명령어 최대 크기
#define NAME_SIZE       30          // 닉네임 최대 길이
#define MAX_SOCKS       100         // 최대 연결 가능 클라이언트 수

#define GETSERVMSG_TIMEOUT_SEC  0
#define GETSERVMSG_TIMEOUT_USEC 50000   // 1000000 usec = 1 sec

#define MIN_ERASE_LINES     1           // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE       3           // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

// 지정된 멀티캐스팅 주소
char mulcast_addr[] = "239.0.100.1";

int client[MAX_SOCKS];                  // 클라이언트 연결 저장 배열
char names[MAX_SOCKS][NAME_SIZE];       // 닉네임 저장 배열

// prompt-print message, 즉 '입력 문구'
char pp_message[] = "Input message(CTRL+C to quit):\r\n";

// command mode message, 즉 cmd mode에서의 입력 문구
char cmd_message[] = "Enter command(ESC to quit):\r\n> ";

// 'emoji' 문자열 임시 내장 - 파일입출력으로의 사용 전까지
char myh[] = "⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀\r\n⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀\r\n⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀\r\n⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀\r\n⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀\r\n⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀\r\n⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀\r\n⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆\r\n⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦\r\n⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷\r\n⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹\r\n⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿\r\n⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷";

// 함수명 변경: error_handling() >> perror_exit()
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
    
    if (servlog) printf("\r\nMESSAGE FROM SERVER: %s\r\n", servlog);

    printf("Total msgsize: %d of %d maximum\r\n", CMDCODE_SIZE + NAME_SIZE + (int)strlen(msg), BUF_SIZE);

    for (i = 0; i < clnt_cnt; i++)
    {
        // DISCARD DISCONNECTED OR NAMELESS CLIENTS
        // 서버에서 나갔거나, 닉네임이 설정되지 않은 클라이언트는 무시한다.
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], message, BUF_SIZE);
        printf("Sent to client [%d] (%s)\r\n", client[i], names[i]);
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
 *   Pull Request의 [other?] 부에 작성한 표를 참고하십쇼!
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
    // 어쩌다 kbhit()까지 확있했는데도 read()할 거 없으면 그냥 그거 반환하기로
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

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    int serv_sock, clnt_sock;
    int state = 0;              // 초기 연결 때의 오류 여부, 그 이후에는 클라이언트 상태 저장 용도로써 사용
    struct ip_mreq join_addr;   // 멀티캐스트 주소 저장
    struct sockaddr_in clnt_addr, serv_addr;

    struct timeval ti;          // timeval_(std)input - 입력값에 대한 타임아웃 구조체
    fd_set readfds, allfds;     // 클라이언트 배열 순회 용도, 둘의 정확한 차이나 용도는 불명
    
    /* buf[]와 message[]의 용도 규칙은 따로 없으나
     * 보통 입력 저장 및 임시 저장소는 buf[],
     * NULL칸으로 분리된 형식의 수신/전송용 메시지는 message[]를 사용.
     * 필요, 흐름, 문맥에 따라 둘의 용도를 '배열1', '배열2'와 같이 교대로 사용하기도 함
     */
    char buf[BUF_SIZE];
    char message[BUF_SIZE];
    char cmd[CMD_SIZE];

    /* sendAll()의 사용 형식에 맞추기 위한 '서버 이름'.
     * sendAll()에 사용할 때는 char *sender 인자에 넣으면 됨
     */
    char serv_name[NAME_SIZE] = "SERVER";
    
    int i, j;
    int clnt_size, clnt_cnt;
    int fd_max;

    /* 필요할 때만 '입력 문구'를 출력하도록 함.
     * 코드 진행 이후 '입력 문구'의 출력이 필요할 때는 prompt_printed = 1 실행
     */
    int prompt_printed = 0;

    /* COMMAND MODE
     * ESC 눌러서 실행
     */
    int cmdmode = 0;

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

    for (int i = 0; i < MAX_SOCKS; i++) client[i] = -1;

    FD_ZERO(&readfds);
    FD_SET(serv_sock, &readfds);

    printf("\nStarted TCP Server.\n\n");

    fflush(0);
    
    //// 이때부터 글자 하나씩 입력받기 모드 시작.
    set_conio_terminal_mode();

    memset(buf, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    memset(cmd, 0, CMD_SIZE);

    int bi = 0;     // buf string index
    int ci = 0;     // cmd string index

    ////// CLIENT INTERACTION LOOP. //////

    while (1)
    {
        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        // 수신 메시지 존재 or 입력 버퍼 비워짐으로 추가 출력이 존재하는 경우에만 '입력 문구' 출력.
        if (!prompt_printed) {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");
            printf("%s", cmdmode ? cmd_message : pp_message);
            prompt_printed = 1;
            fflush(stdout);
        }
        
        // kbhit() 내에서 select() 돌아감, 추가적 select() 필요 없음
        // 그래서 fd_set stdinfd 지움
        if (kbhit())
        {
            int c = getch();

            /* 여긴 cmdmode 여부에 따라 다른 버퍼와 버퍼 인덱스 사용함
             * message : 버퍼 buf, 버퍼 인덱스 bi
             * cmdmode : 버퍼 cmd, 버퍼 인덱스 ci
             * 삼항자 써서 숏코딩 날릴 수도 있지만 여긴 가독성이 더 중요해 보여서
             * 그리고 명령어 추가하다 보면 어차피 다 확장해야 할 듯 싶어서
             */
            switch (c)
            {
                // PRESSED CTRL + C
                // 99 & 037
                case 3:
                {
                    reset_terminal_mode();

                    moveCursorUp(PP_LINE_SPACE, 0);
                    printf("\r\nClosed server.");
                    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");

                    exit(0);
                    break;
                }

                // PRESSED ESC
                case 27:
                {
                    if (cmdmode)
                    {
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1);
                        cmdmode = 0;
                    }

                    else
                    {
                        memset(cmd, ci = 0, CMD_SIZE);
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
                case 17: case 18: case 19: case 20:
                {
                    // UP, DOWN, RIGHT, LEFT
                    // please don't enable the arrow keys  yet

                    break;
                }

                // PRESSED BACKSPACE
                case 127:
                {
                    if (cmdmode && ci > 0)
                    {
                        printf("\b \b");
                        cmd[--ci] = 0;
                    }

                    else if (bi > 0)
                    {
                        printf("\b \b");
                        buf[--bi] = 0;
                    }
                    
                    break;
                }

                // PRESSED CTRL + BACKSPACE
                case 8:
                {
                    int c = 0;

                    if (cmdmode)
                    {
                        for (ci; ci > 0 && cmd[ci - 1] == ' '; printf("\b \b"), cmd[--ci] = 0, c++);
                        for (ci; ci > 0 && cmd[ci - 1] != ' '; printf("\b \b"), cmd[--ci] = 0, c++);
                        for (ci; ci > 0 && cmd[ci - 1] == ' '; printf("\b \b"), cmd[--ci] = 0, c++);
                    }

                    else
                    {
                        for (bi; bi > 0 && buf[bi - 1] == ' '; printf("\b \b"), buf[--bi] = 0, c++);
                        for (bi; bi > 0 && buf[bi - 1] != ' '; printf("\b \b"), buf[--bi] = 0, c++);
                        for (bi; bi > 0 && buf[bi - 1] == ' '; printf("\b \b"), buf[--bi] = 0, c++);
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
                    if (cmdmode)
                    {
                        // 
                        // DO SOMETHING WITH THE COMMAND HERE...
                        // 명령에 따라 클라이언트 목록 뽑든가
                        // cmd[ci] ...
                        // 
                    }

                    else
                    {
                        // CHECK FOR EMOJIS
                        // 아래에 대한 구체적 주석은 emojis 브랜치의 check_append_emojis()에 작성할 것.
                        // 아래 내용은 해당 함수의 사용으로 대체될 것.
                        char mdest[BUF_SIZE], umdest[BUF_SIZE] = "\r\nMESSAGE FROM SERVER:\r\n";
                        char *index = strstr(buf, ":myh:");
                        if (index)
                        {
                            // CAST TO INT FOR MEMORY LOCATION INDICATION
                            // GET THE SIZE VALUE INBETWEEN
                            int inbet = (int)index - (int)buf;
                            
                            memcpy(mdest, buf, inbet);
                            sprintf(&mdest[inbet], "\r\n%s\r\n%s", myh, &buf[inbet + sizeof(":myh:") - 1]);
                            strcat(umdest, mdest);
                        }

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1);

                        // SEND
                        if (index)  sendAll(clnt_cnt, 1000, serv_name, umdest, mdest);
                        else        sendAll(clnt_cnt, 1000, serv_name, buf, buf);

                        memset(buf, bi = 0, BUF_SIZE);
                        prompt_printed = 0;
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
                        buf[bi++] = '\r';
                        buf[bi++] = '\n';
                    }

                    break;
                }
                
                // PRESSED OTHER
                default:
                {
                    printf("%c", c);

                    if (cmdmode)
                        cmd[ci++] = c;
                    else
                        buf[bi++] = c;

                    break;
                }
            }

            fflush(stdout);
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
            printf("Connection from (%s, %d)\r\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
            prompt_printed = 0;

            for (i = 0; i < MAX_SOCKS; i++)
            {
                if (client[i] < 0) {
                    client[i] = clnt_sock;
                    printf("Client number: %d\r\n", i + 1);
                    printf("Client FD: %d\r\n", clnt_sock);
                    break;
                }
            }

            printf("Accepted [%d]\r\n", clnt_sock);
            printf("===================================\r\n");

            if (i == MAX_SOCKS) perror("Too many clients!\r\n");

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
                    printf("Disconnected client [%d] (%s)\r\n", client[i], names[i]);
                    printf("===================================\r\n");
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
                    printf("\r\nReceived from [%d] (%s): %s %s\r\n", client[i], names[i], buf, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
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
                            printf("Set name of client [%d] as [%s]\r\n", client[i], names[i]);

                            // SEND JOIN INFORMATION TO ALL CLIENTS
                            memset(message, 0, BUF_SIZE);
                            sprintf(message, "%s joined the chat!", names[i]);
                            sendAll(clnt_cnt, 1000, serv_name, message, message);
                        }
                    }

                    //// MODE : MESSAGE ////

                    else if (cmdcode == 3000)
                    {
                        char msg[BUF_SIZE], mdest[BUF_SIZE];
                        strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                        
                        // CHECK FOR EMOJIS
                        // 아래에 대한 구체적 주석은 emojis 브랜치의 check_append_emojis()에 작성할 것.
                        // 아래 내용은 해당 함수의 사용으로 대체될 것.
                        char *index = strstr(msg, ":myh:");
                        if (index)
                        {
                            // CAST TO INT FOR MEMORY LOCATION INDICATION
                            // GET THE SIZE VALUE INBETWEEN
                            int inbet = (int)index - (int)msg;

                            memcpy(mdest, msg, inbet);
                            sprintf(&mdest[inbet], "\r\n%s\r\n%s", myh, &msg[inbet + sizeof(":myh:") - 1]);
                            printf("%s\r\n", mdest);
                            fflush(stdout);
                        }

                        // SEND RECEIVED MESSAGE TO ALL CLIENTS
                        memset(message, 0, BUF_SIZE);
                        sendAll(clnt_cnt, 3000, names[i], index ? mdest : msg, NULL);
                    }

                    else
                    {
                        printf("Error reading cmdcode from [%d]!\r\n", client[i]);
                    }
                }

                memset(buf, bi = 0, BUF_SIZE);

                if (--state <= 0) break;
            }
        }
    }
}
