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

// 이중 연결 리스트 api
// https://github.com/clibs/list
#include "list/list.h"

//// 마지막에 아래 정리해서 아래 변수들 함수들 헤더랑 따로 만들어서 담을 것

#define BUF_SIZE            1024 * 10   // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define MSG_SIZE            1000 * 10
#define CMDCODE_SIZE        4           // cmdcode의 크기
#define CMD_SIZE            20          // cmdmode에서의 명령어 최대 크기
#define NAME_SIZE           30          // 닉네임 최대 길이
#define MAX_SOCKS           100         // 최대 연결 가능 클라이언트 수
#define ACCEPT_MSG_SIZE     5           // 1 + sizeof(int)

#define RECV_TIMEOUT_SEC        0
#define RECV_TIMEOUT_USEC       50000   // 1000000 usec = 1 sec
#define SEND_TIMEOUT_SEC        0
#define SEND_TIMEOUT_USEC       100000

#define MIN_ERASE_LINES         1       // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE           3       // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

#define SERVMSG_CMD_CODE        1000

#define HEARTBEAT_CMD_CODE      1500
#define HEARTBEAT_REQ_CODE      1501
#define HEARTBEAT_STR_CODE      1502

#define SINGLECHAT_REQ_CODE     1600
#define SINGLECHAT_RESP_CODE    1601
#define CHANCHAT_REQ_CODE       1602
#define CHANCHAT_RESP_CODE      1603

#define SETNAME_CMD_CODE        2000

#define OPENCHAT_CMD_CODE       3000
#define SINGLECHAT_CMD_CODE     3001

#define SERVCLOSED_CMD_CODE     4000

int global_curpos = 0;

// COMMAND MODE: ESC 눌러서 실행
int cmdmode;

int MEMBER_SRL = -1;    // -1: 미지정

char NNAME[NAME_SIZE];  // 자기 자신의 닉네임

// 처음에 '0'이 아니라 '-1' 되도록 수정함: client 번호는 0부터 시작하기 때문.
int CHAT_TARGET = -1;   // 타겟 번호. 개인채팅이면 타겟 member_srl, 단체면 channel

int CHAT_STATUS = 0;    // idle = 0, personal_chat = 1, channel_chat = 2

int named_client_count = 0;

// 지정된 멀티캐스팅 주소
char TEAM_MULCAST_ADDR[] = "239.0.100.1";

int sock;               // 서버 소켓

// prompt-print message, 즉 '입력 문구'
char pp_message[] = "Input message(CTRL+C to quit CHAT):\r\n";

// command mode message, 즉 cmd mode에서의 입력 문구
char cmd_message[] = "Enter command(ESC to exit CMDMODE):\r\n> ";

// 실제 입력 관리: 이중 리스트 api를 사용하였다.
// 엔터를 쳤을 때 blist 또는 clist의 데이터를 buf[]로 저장하고,
// 해당 리스트를 초기화한다.
list_t      *blist;     // buf_list
list_node_t *bp;        // buf_list_pointer

list_t      *clist;     // cmd_list
list_node_t *cp;        // cmd_list_pointer

struct sClient
{
    int logon_status;           // logon 되어있으면 1, 아니면 0
    char nick[NAME_SIZE];
    int chat_status;            // idle = 0, personal_chat = 1, channel_chat = 2
    int target;                 // 타겟 번호. 개인채팅이면 타겟 member_srl, 단체면 channel
    int is_chatting;            // 채팅 중인지
    time_t last_heartbeat_time; // 마지막 heartbeat을 받은 시간

} client_data[MAX_SOCKS];       // member_srl은 client_data[i] 에서 i이다.

struct HeartBeatPacket
{
    int cmd_code;       // HEARTBEAT 전송 CMDCODE
    int member_srl;     // 클라이언트 고유번호
    int chat_status;    // 채팅 상대가 있는지 (idle = 0, personal_chat = 1, channel_chat = 2)
    int target;         // 채팅 상대
    int is_chatting;    // 타이핑 중인지
};

// 함수명 변경: error_handling() >> perror_exit()
void perror_exit(char *message)
{
    printf("%s", message);
    exit(0);
}

// integer to ascii
void itoa(int i, char *st)
{
    sprintf(st, "%d", i);
    return;
}

// MESSAGE SENDER
void send_msg(int cmdcode, char *msg)
{
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);

    sprintf(message, "%d", cmdcode);
    sprintf(&message[CMDCODE_SIZE], "%s", NNAME);
    sprintf(&message[CMDCODE_SIZE + NAME_SIZE], "%s", msg);

    write(sock, message, BUF_SIZE);
}

void send_singlechat(char *message)
{
    char result[BUF_SIZE] = {0,};
    memset(&result, 0, BUF_SIZE);

    char tmp[5] = {0,};
    int offset = 0;

    itoa(SINGLECHAT_CMD_CODE, tmp);
    memcpy(&result, &tmp, sizeof(int));
    offset = sizeof(int);

    itoa(MEMBER_SRL, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));
    offset += sizeof(int);

    itoa(CHAT_TARGET, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));
    offset += sizeof(int);

    memcpy(&result[offset], message, strlen(message));

    write(sock, result, BUF_SIZE);
}

// MESSAGE RECEIVER: ALSO SETS PARAMETER VALUES TO 0
// RETURNS read() RESULT
/* 
 * int *cmdcode: cmdcode값을 직접 수정해 주기 때문에 포인터로 받는다.
 * 각 크기 변수를 배열 인덱스로 활용해, 메시지 데이터들을 인자로 주어진 각 변수에 저장한다.
 */
// NULL 문자 삽입 없앰
int recv_msg(int *cmdcode, char *sender, char *message)
{
    char buf[BUF_SIZE];
    int rresult;

    memset(sender, 0, NAME_SIZE);
    memset(buf, 0, MSG_SIZE);
    memset(message, 0, BUF_SIZE);

    if ((rresult = read(sock, buf, BUF_SIZE)) < 0) return rresult;

    char tmp[5] = {0,};
    int offset = 0;

    memcpy(tmp, buf, sizeof(int));
    offset = sizeof(int);
    *cmdcode = atoi(tmp);

    memcpy(sender, &buf[offset], sizeof(sender));
    offset += sizeof(sender);

    memcpy(message, &buf[offset], sizeof(message));

    return rresult;
}

void send_singlechat_request(int member_srl)
{
    char pass[CMDCODE_SIZE * 2] = {0,};

    sprintf(pass, "%d", SINGLECHAT_REQ_CODE);
    sprintf(&pass[CMDCODE_SIZE], "%d", member_srl);
    write(sock, pass, CMDCODE_SIZE * 2);
}

void send_singlechat_response(int member_srl, int accepted)
{
    char pass[CMDCODE_SIZE * 3] = {0,};

    sprintf(pass, "%d", SINGLECHAT_RESP_CODE);
    sprintf(&pass[CMDCODE_SIZE], "%d", member_srl);
    sprintf(&pass[CMDCODE_SIZE * 2], "%d", accepted);
    write(sock, pass, CMDCODE_SIZE * 3);
}


// GET LINEFEED COUNT OF BUFFER LIST
/*
 * 한 버퍼 리스트에 포함된 줄넘김의 수 구함
 * it's always better to NOT modify the "original" source.
 : adding list->(unsigned int lfcnt) in this case.
 */
int getLFcnt(list_t* list)
{
    list_node_t* node;
    list_iterator_t* it = list_iterator_new(list, LIST_HEAD);

    int lfcnt = 0;
    while (node = list_iterator_next(it)) if (node->val == '\n') lfcnt++;
    list_iterator_destroy(it);

    return lfcnt;
}

int getLFcnt_from_node(list_node_t* list_ptr, int dirFrom)
{
    list_node_t* node;
    list_iterator_t* it = list_iterator_new_from_node(list_ptr, dirFrom);

    int lfcnt = 0;
    while (node = list_iterator_next(it)) if (node->val == '\n') lfcnt++;
    list_iterator_destroy(it);

    return lfcnt;
}

// PRINT DATA AFTER MODIFIED NODE BEHIND CURSOR
/*
 * 노드 추가/삭제 후 [노드->next]의 값들을 커서 뒤로 모두 출력
 */
void print_behind_cursor(list_t* list, list_node_t* list_ptr, char firstchar, char lastchar, int lastcharcnt)
{
    list_node_t *node = list_ptr;
    int restlen = 1;

    if (firstchar) printf("%c", firstchar);
    while (node = node->next)
    {
        if (node->val == '\r') break;
        printf("%c", node->val);
        restlen++;
        if (node == list->tail) break;
    }

    for (int i = 0; i < lastcharcnt; i++) printf("%c", lastchar);
    printf(" \033[%dD", restlen + lastcharcnt);
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
void moveCursorUp(int lines, int eraselast, list_node_t* list_ptr)
{
    // 버퍼 리스트 포인터가 인자로 주어진 경우 **위로** 지우기 전에 **밑으로** 먼저 내린다.
    if (list_ptr) printf("\033[%dB", getLFcnt_from_node(list_ptr, LIST_HEAD));

    // eraselast가 설정된 경우, 커서를 윗줄로 올리기 전에 커서가 있던 줄을 지우고 간다.
    if (eraselast) printf("\33[2K");

    // 커서 위 lines개의 줄을 지운다.
    for (int i = 0; i < lines; i++) printf("\033[A\33[2K");

    // \033[A는 커서를 처음으로 옮겨 주지 않으므로, 커서를 맨 앞으로 옮겨 준다.
    printf("\r");

    // 출력 버퍼 비움
    fflush(stdout);
}

// USAGE OF HORIZONTAL CURSOR MOVEMENT - BY BLOCKS
/*
 * CTRL + MOVEKEY : 단어 단위로 커서 이동
 * CTRL + ERASEKEY: 단어 단위로 버퍼 데이터 지움
 *
 * 장점:
 *   일단 보고 쓰기 확 편해짐.
 *   공백 순서 같은 거 수정하고 싶을 때 8곳에서 수정해 주지 않아도 됨.
 * 
 * int modifying: 주어진 list_t *list의 데이터를 함께 수정하는지 여부
 *   0: CTRL + MOVEKEY
 *   1: CTRL + ERASEKEY
 * 
 * int dirFrom:
 *   리스트의 dirFrom의 반대까지 확인
 *   원래 dirTo였으나 다른 리스트 사용 함수들과의 통일성을 위해 dirFrom으로 다시 뒤집음.
 *   0: LIST_HEAD
 *   1: LIST_TAIL
 * 
 * 이를 이용한 코드 구조:
 *   dirFrom ? [HEAD 방향으로 확인하는 경우] : [TAIL 방향으로 확인하는 경우]
 * 
 * tp != (dirFrom ? list->head : list->tail):
 * 원래 list_node_t *end = (dirFrom ? list->head : list->tail)로 초기조건 변수를 만들어 저장하려 했으나
 * 각 반복문이 매 바퀴를 돌 때마다 list->tail값이 변화되기 때문에
 * end가 가리킬 위치를 매번 갱신해 주지 않으면, 줄의 맨 끝에서 [CTRL + DELETE]를 눌렀을 때
 * Segmentation fault (core dumped)가 발생한다.
 */
list_node_t* moveCursorColumnblock(list_t *list, list_node_t *p, char *printstr, int modifying, int dirFrom)
{
    int i = 0;
    list_node_t *tp = p;

    while (tp != (dirFrom ? list->head : list->tail) && (dirFrom ? tp->val != '\n' : tp->next->val != '\r') && (dirFrom ? tp->val == ' ' : tp->next->val == ' '))
    {
        if (printstr) { printf("%s", printstr); tp = (dirFrom ? tp->prev : tp->next); }
        if (modifying) list_remove(list, tp->next);
        i++;
    }

    while (tp != (dirFrom ? list->head : list->tail) && (dirFrom ? tp->val != '\n' : tp->next->val != '\r') && (dirFrom ? tp->val != ' ' : tp->next->val != ' '))
    {
        if (printstr) { printf("%s", printstr); tp = (dirFrom ? tp->prev : tp->next); }
        if (modifying) list_remove(list, tp->next);
        i++;
    }

    if (modifying) print_behind_cursor(list, tp, 0, ' ', i);

    return tp;
}

int getCurposFromListptr(list_t* list, list_node_t* list_ptr)
{
    int curpos = 0;

    list_node_t* node;
    list_iterator_t* it;

    node = list_ptr;
    
    it = list_iterator_new_from_node(node, LIST_TAIL);

    while (node = list_iterator_next(it))
    {
        if (node == list->head) break;
        if (node->val == '\n') break;
        curpos++;
    }
    list_iterator_destroy(it);

    // list_ptr 에서 그 전 마지막 줄넘김까지의 거리
    return curpos;
}

// 줄넘김 들어간 MODIFYING 있을 때, 즉 !cmdmode일 때 사용.
// 그냥 전체 다 뽑아 버린다
void eraseInputSpace(list_t* list, list_node_t* list_ptr)
{
    printf("\033[%dB", getLFcnt_from_node(list_ptr, LIST_HEAD));
    moveCursorUp(getLFcnt(list), 1, list_ptr);
}

void reprintList(list_t* list, list_node_t* list_ptr, int curpos)
{
    list_node_t *node;
    list_iterator_t *it;

    // 뽑아.
    // 출력 끝난 후에 커서 위치: 맨 아랫줄.
    it = list_iterator_new(list, LIST_HEAD);
    while (node = list_iterator_next(it)) printf("%c", node->val);
    list_iterator_destroy(it);

    int lastlf = 0, ptrpos = 0;

    // 커서 뒤에 줄넘김이 있을 때마다 한 줄씩 올려준다.
    int lfcnt = getLFcnt_from_node(list_ptr, LIST_HEAD);
    if (lfcnt) printf("\033[%dA", lfcnt);

    // 커서가 줄넘김한 직후, 즉 해당 줄의 맨 앞 위치일 때
    // 위 반복문의 if (node->val == '\n')에서 한 번 더 올라간 커서를 다시 내려 준다
    if (list_ptr->val == '\n') printf("\033[B");

    printf("\r");
    if (curpos) printf("\033[%dC", curpos);
}

// PRINT UNTIL CURSOR TO END OF LINE
// >> PRINT UNTIL CURSOR TO END OF BUFFER LIST
/*
 * 리스트를 돌며 줄넘김 전까지 char *printstr를 출력하기
 *
 * int dirFrom:
 *   리스트의 dirFrom에서 출발
 *   0: LIST_HEAD: 리스트의 tail 방향으로 반복문 돌아감
 *   1: LIST_TAIL: 리스트의 head 방향으로 반복문 돌아감
 */
void printUntilEnd(list_t* buflist, list_node_t* list_ptr, char *printstr, int modifying, int dirFrom)
{
    list_node_t *node;

    node = list_ptr;

    if (dirFrom == LIST_HEAD)
    {
        while (node != buflist->tail && node->next->val != '\n')
        {
            printf("%s", printstr);
            node = node->next;
        }
    }

    else  // dirFrom LIST_TAIL
    {
        while (node != buflist->head && node->val != '\n')
        {
            printf("%s", printstr);
            node = node->prev;
        }
    }
}

// CHECK LIST IS "LITERALLY" EMPTY
/*
 * 버퍼 리스트에 실제 데이터 값이 들어 있는지 확인
 *
 * 다음의 경우에 1 반환:
 *   리스트가 비어 있음
 *   리스트의 모든 노드의 val 값이 0, 공백, \r, \n 중 하나임
 * 
 * 리스트에 실제 의미 있는 값이 들어가 있으면 1 반환.
 */
int list_is_empty(list_t* list)
{
    if (list->head == list->tail) return 1;

    list_node_t *node;
    list_iterator_t *it = list_iterator_new(list, LIST_HEAD);

    while (node = list_iterator_next(it))
    {
        if (node->val != 0 && node->val != ' ' && node->val != '\r' && node->val != '\n')
        {
            list_iterator_destroy(it);
            return 0;
        }
    }

    list_iterator_destroy(it);
    return 1;
}

// LIST DATA TO BUFFER
/* 
 * 인수들
 * 
 * char     *buf      : 리스트 데이터를 저장할 배열
 * list_t   *list     : 읽을 리스트
 * int       emptylist: 리스트 초기화 여부
 */
list_node_t* transfer_list_data(char *buf, list_t *list, int emptylist)
{
    list_node_t *node;
    list_iterator_t *it = list_iterator_new(list, LIST_HEAD);

    int offset = 0;

    // 각 노드에 단일 char가 저장되어 있는 list의 입력 데이터를
    // buf 배열에 저장!
    while (node = list_iterator_next(it))
    {
        if (node->val == 0) continue;
        sprintf(&buf[offset++], "%c", node->val);
    }
    list_iterator_destroy(it);

    //// 리스트 내용 복사 완료 ////

    if (emptylist)
    {
        list_destroy(list);
        list = list_new();

        list_rpush(list, list_node_new(0));
    }

    return list->head;
}

// INSERT VALUE IN THE MIDDLE OF A BUFFER LIST
/* 
 * 리스트 중간에 삽입한다
 * 
 * 버그:
 *   문자 4글자 이하, 그 4글자들 사이에
 *   [ALT + ENTER] & [BACKSPACE] 3번 이상 치고 넘겼을 때
 *   Segmentation fault (core dumped) 현상 발생
 * 
 * 해결
 *   (list->len)++
 * 
 * list/list.c 내 디버깅 코드:
 * void list_destroy(list_t *self)에서
 * 
 * // for core dump debugging
 * // printf("[not yet, len: %d]\r\n", len);
 * 
 + 보니까 bllen, cllen 따로 만들지 않아도 list_t* 구조체에
 | len이라는 내장 변수 있어서 그냥 그거 쓰면 되었음
 V 
 : list->len
 * 
 * not incrementing it turned out to be the f reason of the f core dumping
 * incrementing it in [if (list_ptr == list->tail)] ALSO turns out to result in core dumping
 * 'cause list->len is already incremented IN list_rpush
 * .
 * .
 * [언짢으면서도 유익한 경험이었 다]
 */
list_node_t* list_insert(list_t* list, list_node_t* list_ptr, char newvalue)
{
    list_node_t *newnode = list_node_new(newvalue);

    if (list_ptr == list->tail) list_rpush(list, newnode);

    else
    {
        (newnode->next = list_ptr->next)->prev = newnode;
        (newnode->prev = list_ptr)->next = newnode;
        list->len ++;
    }

    return newnode;
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
    // 키 하나 눌렀을 때 반환되는 정수들
    // 추가 키 사용할 때마다 그 키의 반환 개수에 따라 배열 길이 늘려서 사용할 수 있음!
    char c[7] = {0,};

    // 어쩌다 kbhit()까지 확인했는데도 read()할 거 없으면 그냥 그거 반환하기로
    int r;
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
                    if (!strcmp(&c[1], "[3~")) return 128;
                    break;

                // CTRL + KEY
                case 6:
                    // CTRL + ARROWS
                    if (c[5] > 64 && c[5] < 69) return -1 * c[5] + 48;

                    // CTRL + DELETE
                    if (!strcmp(&c[1], "[3;5~")) return -128;
                    break;

                // 그 외의 (더 많은 정수들을 반환하는) COMMAND KEY 조합
                // 더 쓸 거 있으면 추가할 것
                // 원래 키의 음수값 * 10으로 사용하도록 지정했음
                // 그니까 먹지 마셈
                default:
                    return -10 * c[strlen(c) - 1];
                    break;
            }
        }

        else
            return c[0];
    }
}

void heartbeatSerialize(char *message, struct HeartBeatPacket *hbp)
{
    char result[BUF_SIZE] = {0,};
    memset(&result, 0, BUF_SIZE);

    char tmp[5] = {0,};
    int offset = 0;

    itoa(HEARTBEAT_CMD_CODE, tmp);
    memcpy(&result, &tmp, sizeof(int));
    offset = sizeof(int);

    itoa(hbp->member_srl, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));
    offset += sizeof(int);

    itoa(hbp->chat_status, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));
    offset += sizeof(int);

    itoa(hbp->target, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));
    offset += sizeof(int);

    itoa(hbp->is_chatting, tmp);
    memcpy(&result[offset], &tmp, sizeof(int));

    memcpy(message, &result, BUF_SIZE); // 최종 메세지 저장
}

void clientListProcess(char* message)
{
    char tmp[5]={0,};
    int offset = 0;

    while(1)
    {
        memcpy(&tmp, &message[offset], sizeof(int));
        offset += sizeof(int);
        int cmd_code = atoi(tmp);

        if(cmd_code == HEARTBEAT_STR_CODE) // Heartbeat가 있을 경우
        {
            memcpy(&tmp, &message[offset], sizeof(int));
            offset += sizeof(int);
            int member_srl = atoi(tmp);

            memcpy(&client_data[member_srl].nick, &message[offset], sizeof(client_data[member_srl].nick));
            offset += sizeof(client_data[member_srl].nick);

            memcpy(&tmp, &message[offset], sizeof(int));
            offset += sizeof(int);
            client_data[member_srl].logon_status = atoi(tmp);

            memcpy(&tmp, &message[offset], sizeof(int));
            offset += sizeof(int);
            client_data[member_srl].chat_status = atoi(tmp);

            memcpy(&tmp, &message[offset], sizeof(int));
            offset += sizeof(int);
            client_data[member_srl].target = atoi(tmp);

            memcpy(&tmp, &message[offset], sizeof(int));
            offset += sizeof(int);
            client_data[member_srl].is_chatting = atoi(tmp);
        } else break;
    }
}

// 키보드 작성중인지 확인
int isKeyboardWriting()
{
    return !list_is_empty(blist);
}

void print_available_clients(int isFirstPrint)
{
    if (!isFirstPrint) moveCursorUp(named_client_count + 4, 1, 0);
    else printf("\r\n");

    int i;
    for (i = 0; i < MAX_SOCKS && client_data[i].nick[0]; i++)
        printf("SRL \033[1m%d\033[0m (%s)\r\n", i, client_data[i].nick);
    printf("\033[1mTOTAL NAMED CLIENTS:\033[0m %d\r\n", i);

    named_client_count = i;
    // printf("%s", cmd_message);
    fflush(0);
}

// 메인화면 출력
// print_main_scene()으로 함수명 바꿀 것.
void firstScene()
{
    // 채팅상태 idle
    CHAT_STATUS = 0;

    printf("\033[1;33m--------------------------------------------------------------\033[0m\r\n");
    printf("\033[1;33m==================== Welcome To The CHAT! ====================\033[0m\r\n");
    printf("\r\n\n");

    printf("\033[1;33mNICKNAME:\033[37m %s\033[0m\r\n\n", NNAME);

    printf("\033[1;33m[Private chatting]\033[0m\r\n\n");
    printf("- \033[1m개인채팅 사용방법:\033[0m 닉네임 앞 숫자 입력");

    // printf("\r\n\n\n");
    // printf("\033[1;33m[Group chatting]\033[0m\r\n\n");
    // printf("- \033[1m그룹채팅 사용방법:\033[0m c(채널번호)    \033[1mex)\033[0m 12번 채널 가입: [c12]\r\n");
    printf("\r\n\n\n");
    printf("\033[1;33m==============================================================\033[0m\r\n");
    printf("\033[1;33m--------------------------------------------------------------\033[0m\r\n");

    fflush(0);
}

void close_client(int servclosed)
{
    reset_terminal_mode();

    close(sock);

    if (cmdmode) moveCursorUp(PP_LINE_SPACE, 1, 0);
    else         moveCursorUp(PP_LINE_SPACE + getLFcnt(blist), 1, bp);

    if (servclosed) printf("\r\n\033[1;4;33mSERVER WAS CLOSED.\033[0m\r\n\n");

    printf("\r\n\033[1;4;33mCLOSED CLIENT.\033[0m");

    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <PORT>\n", argv[0]);
        exit(1);
    }

    int namelen;                // 자기 자신의 닉네임 길이 (닉네임 설정 때 사용)

    int state = 0;              // 초기 연결 때의 오류 여부, 그 이후에는 클라이언트 상태 저장 용도로써 사용
    struct ip_mreq join_addr;   // 멀티캐스트 주소 저장
    struct sockaddr_in mul_addr, serv_addr;

    struct  timeval tr;     // timeval_receive
    fd_set  readfds;        // CONTROLS SELECT [once set: read-only]
    fd_set  allfds;         // SELECT에 의해 변화됨

    /* buf[]와 message[]의 용도 규칙은 따로 없으나
     * 보통 입력 저장 및 임시 저장소는 buf[],
     * NULL칸으로 분리된 형식의 수신/전송용 메시지는 message[]를 사용.
     * 필요, 흐름, 문맥에 따라 둘의 용도를 '배열1', '배열2'와 같이 교대로 사용하기도 함
     */
    char buf[BUF_SIZE];
    char message[BUF_SIZE];
    char cmd[CMD_SIZE];

    blist = list_new();
    list_rpush(blist, list_node_new(0));    // blist->HEAD
    bp = blist->head;

    clist = list_new();
    list_rpush(clist, list_node_new(0));    // clist->HEAD
    cp = clist->head;

    /* 필요할 때만 '입력 문구'를 출력하도록 함.
     * 코드 진행 이후 '입력 문구'의 출력이 필요할 때는 prompt_printed = 0 실행
     */
    int prompt_printed = 0;

    int cmdcode;
    char sender[NAME_SIZE];

    tr.tv_sec  = RECV_TIMEOUT_SEC;
    tr.tv_usec = RECV_TIMEOUT_USEC;

    // JOIN MULTICAST GROUP
    // 주어진 멀티캐스팅 주소로 연결한다.
    int udp_sock;
    int str_len;

    //// UDP 소켓, 연결 설정

    udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) perror_exit("UDP socket() error!\n");

    memset(&mul_addr, 0, sizeof(mul_addr));
    mul_addr.sin_family = AF_INET;
    mul_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    mul_addr.sin_port = htons(atoi(argv[1]));

    join_addr.imr_multiaddr.s_addr = inet_addr(TEAM_MULCAST_ADDR);
    join_addr.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&join_addr, sizeof(join_addr));

    state = bind(udp_sock, (struct sockaddr *)&mul_addr, sizeof(mul_addr));
    if (state == -1) perror_exit("UDP bind() error: Is another client already open in this PC?\r\n");

    printf("\n\033[1;4;33mCONNECTED TO \033[36mUDP\033[33m MULTICAST.\033[0m\n\n");

    // 모든 입력 무시
    set_conio_terminal_mode();

    memset(&buf, 0, sizeof(buf));
    int _serv_port = 0;
    char _serv_ip[20] = {0,};

    printf("\033[1;33mWAITING FOR MULTICAST MESSAGE \033[37m[1 SEC]\033[33m ...\033[0m\r\n\n");

    // 멀티캐스트에서 IP, 포트번호 받아오기
    // 서버는 1초에 한 번씩 서버IP, 포트 정보 전송함
    str_len = recvfrom(udp_sock, buf, BUF_SIZE, 0, 0, 0);
    if (str_len < 0)
    {
        printf("\033[1mFAILED TO RECEIVE MESSAGE!\033[0m\r\n");
        exit(1);
    }

    if (kbhit()) getch();

    int t_offset = 0;

    char temp[20] = {0,};

    memcpy(&temp, &buf, sizeof(int));
    t_offset = sizeof(int);
    _serv_port = atoi(temp);
    memcpy(&_serv_ip, &buf[t_offset], sizeof(_serv_ip));

    moveCursorUp(2, 1, 0);
    printf("\033[1;35m<< RECEIVED    INFO MESSAGE.\033[0m\r\n");
    printf("\033[1;35m<<\033[37m SERVER IP : %s\033[0m\r\n", _serv_ip);
    printf("\033[1;35m<<\033[37m PORT      : %d\033[0m\r\n", _serv_port);
    fflush(0);

    // 입력 모드 재개
    reset_terminal_mode();

    //// TCP 소켓, 연결 설정

    sock = socket(PF_INET, SOCK_STREAM, 0);   
    if (sock == -1) perror_exit("TCP socket() error!\n");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(_serv_ip);
    serv_addr.sin_port = htons(_serv_port);

    state = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (state == -1) perror_exit("TCP connect() error!\n");

    printf("\n\033[1;4;33mCONNECTED TO \033[36mTCP\033[33m SERVER.\033[0m\n");
    printf("\n\n");
    fflush(0);

    /* prompt_printed와 비슷한 역할인데,
     * 입력 조건 메시지가 처음 출력되는 때와 그 다음부터 출력되는 때를 구분짓는다.
     *
     * 닉네임 조건 불충족 문구 출력 위치를 동일 위치로 고정해 주기 위해 존재한다.
     */
    int warned = 0;

    ////// NICKNAME INITIALIZATION LOOP //////

    while (1)
    {
        printf("\033[1;33mNICKNAME:\033[0m ");

        fgets(buf, MSG_SIZE, stdin);

        // 마지막 줄넘김 없애기: '\n' 문자까지 대기 후 (int)0으로 대체
        for (namelen = 0; buf[namelen] != '\n'; namelen++);
        buf[namelen] = 0;
        
        // 그냥 엔터만 넘긴 경우는 무시한다.
        if (namelen < 1) moveCursorUp(1, 0, 0);

        //// 입력받은 버퍼에서 닉네임 조건 충족 여부를 먼저 확인한 후에 서버로 넘긴다.

        else if (buf[0] == ' ')
        {
            moveCursorUp(warned ? 2 : 1, 0, 0);
            printf("Name must not start with a SPACE character...\n");
            if (!warned) warned = 1;
        }

        else if (namelen > NAME_SIZE)
        {
            moveCursorUp(warned ? 2 : 1, 0, 0);
            printf("Name must be shorter than %d characters!\n", NAME_SIZE);
            if (!warned) warned = 1;
        }

        else
        {
            sprintf(message, "%d", SETNAME_CMD_CODE);
            sprintf(&message[CMDCODE_SIZE], "%s", buf);

            write(sock, message, CMDCODE_SIZE + namelen);

            memset(message, 0, BUF_SIZE);
            read(sock, message, ACCEPT_MSG_SIZE);

            if (message[0] == '0')
            {
                moveCursorUp(warned ? 2 : 1, 0, 0);
                printf("Sorry, but the name %s is already taken.\n", buf);
                if (!warned) warned = 1;
            }

            else
            {
                printf("Name accepted.\n");
                memcpy(NNAME, buf, NAME_SIZE);

                MEMBER_SRL = atoi(&message[1]);
                printf("\n\033[1;33mMEMBER_SRL: \033[37m%d\033[0m\n", MEMBER_SRL);

                break;
            }
        }
    }

    for (int i = 1; i < PP_LINE_SPACE; i++) printf("\n");

    int is_init = 1;

    fflush(0);

    // "[SELF] JOINED THE CHAT!"
    read(sock, message, BUF_SIZE);

    //// FIRST 메인 화면 출력

    char listget[BUF_SIZE] = {0,};
    sprintf(listget, "%d", HEARTBEAT_REQ_CODE);
    write(sock, listget, BUF_SIZE);
    read(sock, message, BUF_SIZE);

    printf("\n\033[1;33mMSG:\033[37m %s\033[0m\n", message);
    clientListProcess(message); // 클라이언트 리스트 받아옴

    firstScene();
    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");
    printf("%s", cmd_message);
    fflush(0);

    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(sock, &readfds);

    int waiting_for_server = 0;
    int waiting_for_target = 0;
    int waiting_for_me = 0;

    int req_from = -1;
    int req_to   = -1;

    ////// MESSAGE COMMUNICATION LOOP //////

    // SET TIMEOUT OF read()
    // https://stackoverflow.com/a/2939145
    // 이렇게 하면 아예 소켓 옵션으로 read()에 타임아웃을 걸 수 있어 유용하다.
    // 바로 위 setsockopt()과는 별개임!
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tr, sizeof(tr));

    //// 이때부터 글자 하나씩 입력받기 모드 시작.
    set_conio_terminal_mode();

    memset(buf, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    memset(cmd, 0, CMD_SIZE);

    // int bi = 0;     // buf string index
    // int ci = 0;     // cmd string index

    int servmsg_printed = 0;

    // 시작할 때는 무조건 cmdmode = 1;
    cmdmode = 1;

    //// if (CHAT_TARGET != -1)
    //// : 타겟 설정 안 됨을 의미함!

    while (1)
    {
        // RECEIVE MESSAGE
        // recv_msg()에서 read()를 실행하여 setsockopt으로 설정한 대기 시간만큼 기다린다.
        // 였는데 보편성 위해 그냥 read()로 바꿈
        if (read(sock, message, BUF_SIZE) < 0) {
            // printf("No message.\r\n");
        }

        else
        {
            char cmd_code[5] = {0,};
            memcpy(&cmd_code, &message, sizeof(int));

            cmdcode = atoi(cmd_code);

            switch (cmdcode)
            {
                //// MODE : MEMBERLIST RESPONSE from server ////
                // 처음 가입 시 1째로 수행
                case HEARTBEAT_STR_CODE:
                {
                    clientListProcess(message);

                    // target이 연결 해제되었을 때
                    // 즉 로컬상 CHAT_TARGET은 > -1인데 수신받은 client_data[]에서의 자기 target은 == -1일때
                    if (CHAT_TARGET != -1 && client_data[MEMBER_SRL].target == -1)
                    {
                        CHAT_TARGET = -1;
                        cmdmode = 1;

                        moveCursorUp(1, 1, 0);
                        firstScene();
                        print_available_clients(1);
                        prompt_printed = 0;
                    }

                    // 계속 "처음의(target 없음)" 상태일 때
                    // 처음 가입 시 1째로 수행
                    else if (CHAT_TARGET == -1)
                    {
                        if (is_init) { printf("\r\n\n"); is_init = 0; }
                        moveCursorUp(1, 1, 0);
                        print_available_clients(0);
                        prompt_printed = 0;
                    }

                    break;
                }

                //// MODE : HEARTBEAT REQUEST from server ////
                // 처음 가입 시 2째로 수행
                case HEARTBEAT_CMD_CODE:
                {
                    struct HeartBeatPacket hbp;
                    hbp.cmd_code = HEARTBEAT_CMD_CODE;
                    hbp.member_srl = MEMBER_SRL;
                    hbp.chat_status = CHAT_STATUS;
                    hbp.target = CHAT_TARGET;
                    hbp.is_chatting = isKeyboardWriting();

                    char message[BUF_SIZE] = {0,};
                    heartbeatSerialize(message, &hbp);

                    write(sock, message, BUF_SIZE);

                    char listget[BUF_SIZE] = {0,};
                    sprintf(listget, "%d", HEARTBEAT_REQ_CODE);
                    write(sock, listget, BUF_SIZE);
                    read(sock, message, BUF_SIZE);

                    clientListProcess(message); // 클라이언트 리스트 받아옴

                    break;
                }

                //// MODE : SINGLECHAT REQUEST from client ////
                case SINGLECHAT_REQ_CODE:
                {
                    if (CHAT_TARGET == -1)
                    {
                        req_from = atoi(&message[4]);

                        moveCursorUp(1, 1, 0);
                        printf("\033[1mClient \033[33m%d (%s)\033[37m requested a private chat. Accept?\033[0m [y/n]:\r\n> ", req_from, client_data[req_from].nick);

                        fflush(0);

                        waiting_for_me = 1;
                    }

                    break;
                }

                //// MODE : SINGLECHAT ACCEPT RESPONSE from client ////
                case SINGLECHAT_RESP_CODE:
                {
                    if (CHAT_TARGET == -1)
                    {
                        if (waiting_for_server)
                        {
                            waiting_for_server = 0;

                            moveCursorUp(1, 1, 0);

                            if (atoi(&message[CMDCODE_SIZE * 2]))  // Is existing client
                            {
                                printf("\033[1;33m%d (%s)\033[37m is an existing client. Waiting for response...\033[0m\r\n", req_to, client_data[req_to].nick);
                                fflush(0);

                                waiting_for_target = 1;
                            }

                            else
                            {
                                printf("\033[1;33m%d\033[37m is not an existing client.\033[0m\r\n> ", req_to);
                                fflush(0);
                            }
                        }

                        else if (waiting_for_target)
                        {
                            waiting_for_target = 0;

                            int accepted = atoi(&message[CMDCODE_SIZE * 2]);

                            if (accepted)
                            {
                                moveCursorUp(0, 1, 0);
                                printf("\033[1;33m%d (%s)\033[37m accepted the chat request.\033[0m\r\n", req_to, client_data[req_to].nick);
                                fflush(0);

                                client_data[MEMBER_SRL].target = req_to;
                                client_data[req_to].target = MEMBER_SRL;
                                CHAT_STATUS = 1;
                                CHAT_TARGET = req_to;

                                // "SERVER MESSAGE" 처럼 취급
                                printf("\r\n\n\n");
                                printf("\033[1;33m============ Started private chat! ============\033[0m");
                                printf("\r\n\n\n");
                                servmsg_printed = 1;

                                // 이제 채팅할 수 있다.
                                cmdmode = 0;
                                prompt_printed = 0;
                            }

                            else
                            {
                                moveCursorUp(1, 1, 0);
                                printf("\033[1;33m%d (%s)\033[37m declined the chat request.\033[0m\r\n> ", req_to, client_data[req_to].nick);
                                fflush(0);
                            }
                        }
                    }

                    break;
                }

                //// MODE : MESSAGE ////
                case SERVMSG_CMD_CODE:
                case OPENCHAT_CMD_CODE:
                case SINGLECHAT_CMD_CODE:
                {
                    if (CHAT_TARGET == -1) break;

                    if (cmdmode) global_curpos = getCurposFromListptr(clist, cp);
                    else global_curpos = getCurposFromListptr(blist, bp);

                    // PRINT MSG AFTER REMOVING PREVIOUS LINES
                    moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                    if (is_init) { printf("\r\n"); is_init = 0; }

                    // CODE 1000: MESSAGE FROM SERVER
                    if (cmdcode == SERVMSG_CMD_CODE)
                    {
                        // 이전 메시지도 서버 메시지일 경우 줄넘김 간격 하나 줄여줌
                        // 즉 클라이언트에서 서버로(그 반대도 포함) 전송자가 바뀐 경우에만 줄넘김 좀 더 넓혀 줌
                        if (servmsg_printed)
                        {
                            moveCursorUp(1, 1, 0);
                        }

                        else
                        {
                            printf("\r\n\n\n");
                            servmsg_printed = 1;
                        }

                        if (strstr(&message[CMDCODE_SIZE + NAME_SIZE], "\033[33m")) printf("\033[33m");
                        printf("\033[1m============ %s ============\033[0m\r\n\n\n", &message[CMDCODE_SIZE + NAME_SIZE]);
                    }

                    // CODE 3001: 개인 채팅
                    else
                    {
                        if (servmsg_printed) servmsg_printed = 0;

                        if (cmdcode == SINGLECHAT_CMD_CODE)
                        {
                            printf("\r\n\033[1m%s\033[0m : %s\r\n", client_data[CHAT_TARGET].nick, &message[CMDCODE_SIZE * 3]);
                            fflush(stdout);
                        }
                    }

                    prompt_printed = 0;

                    break;
                }

                //// MODE : SERVER IS CLOSED ////
                case SERVCLOSED_CMD_CODE:
                {
                    close_client(1);
                    return 0;

                    break;
                }

                //// FAILED TO IDENTIFY CMDCODE ////
                default:
                {
                    printf("\033[1;31mUndefined\033[37m cmdcode from server: %d\033[0m\r\n", cmdcode);

                    break;
                }
            }
        }

        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        // 수신 메시지 존재 or 입력 버퍼 비워짐으로 추가 출력이 존재하는 경우에만 '입력 문구' 출력.
        if (!prompt_printed)
        {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");

            printf("%s", cmdmode ? cmd_message : pp_message);

            if (cmdmode) reprintList(clist, cp, global_curpos + 2);
            else reprintList(blist, bp, global_curpos);

            prompt_printed = 1;

            fflush(stdout);
        }

        fflush(0);

        FD_ZERO(&readfds);
        FD_SET(0, &readfds);  // fd 0 = stdin

        // kbhit() 내에서 select() 돌아감, 추가적 select() 필요 없음
        // 그래서 timeval ts 지움
        if (kbhit())
        {
            int c = getch();

            // cmdmode 여부에 따라 다른 리스트와 버퍼 배열 사용.
            /* message : 입력 리스트 blist, 리스트 노드 수 blist->len, 버퍼 배열 buf 
             * cmdmode : 입력 리스트 clist, 리스트 노드 수 clist->len, 버퍼 배열 cmd
             * 현재로서 cmdmode에서 줄넘김은 사용하지 않는 것으로 지정.
             */
            switch (c)
            {
                // PRESSED CTRL + C
                // 99 & 037
                case 3:
                {
                    close_client(0);

                    return 0;
                }

                // PRESSED ESC
                case 27:
                {
                    if (cmdmode)
                    {
                        // 타겟 있어야 나갈 수 있음!
                        if (CHAT_TARGET == -1) break;

                        //// cmdmode에서 나갈 때

                        // 입력 버퍼 리스트 초기화
                        list_destroy(clist);
                        clist = list_new();
                        list_rpush(clist, list_node_new(0));
                        cp = clist->head;

                        // 입력 버퍼 배열 초기화
                        memset(cmd, 0, CMD_SIZE);

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                        global_curpos = getCurposFromListptr(blist, bp);
                        cmdmode = 0;
                    }

                    else
                    {
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + getLFcnt(blist), 1, bp);
                        global_curpos = 0;
                        cmdmode = 1;
                    }

                    prompt_printed = 0;
                    break;
                }

                // UP ARROW [↑]
                case 17:
                {
                    // printf("UP");
                    break;
                }

                // DOWN ARROW [↓]
                case 18:
                {
                    // printf("DOWN");
                    break;
                }

                // LEFT ARROW [←]
                case 20:
                {
                    if (cmdmode && cp != clist->head)
                    {
                        printf("\033[D");
                        cp = cp->prev;
                    }

                    else if (bp != blist->head)
                    {
                        if (bp->val == '\n')
                        {
                            printf("\033[A");
                            bp = bp->prev->prev;
                            printUntilEnd(blist, bp, "\033[C", 0, LIST_TAIL);
                        }

                        else
                        {
                            printf("\033[D");
                            bp = bp->prev;
                        }
                    }

                    break;
                }

                // RIGHT ARROW [→]
                case 19:
                {
                    if (bp != blist->tail)
                    {
                        if (bp->next->val == '\r')
                        {
                            printf("\033[B\r");
                            bp = bp->next->next;
                        }

                        else
                        {
                            printf("\033[C");
                            bp = bp->next;
                        }
                    }
                    break;
                }

                // CTRL + LEFT ARROW [←]
                case -20:
                {
                    if (cmdmode)    cp = moveCursorColumnblock(clist, cp, "\b", 0, LIST_TAIL);
                    else            bp = moveCursorColumnblock(blist, bp, "\b", 0, LIST_TAIL);
                    break;
                }

                // CTRL + RIGHT ARROW [→]
                case -19:
                {
                    if (cmdmode)    cp = moveCursorColumnblock(clist, cp, "\033[C", 0, LIST_HEAD);
                    else            bp = moveCursorColumnblock(blist, bp, "\033[C", 0, LIST_HEAD);
                    break;
                }

                // PRESSED CTRL + BACKSPACE
                case 8:
                {
                    if (cmdmode)    cp = moveCursorColumnblock(clist, cp, "\b", 1, LIST_TAIL);
                    else            bp = moveCursorColumnblock(blist, bp, "\b", 1, LIST_TAIL);
                    break;
                }

                // PRESSED CTRL + DELETE
                case -128:
                {
                    if (cmdmode)    moveCursorColumnblock(clist, cp, 0, 1, LIST_HEAD);
                    else            moveCursorColumnblock(blist, bp, 0, 1, LIST_HEAD);
                    break;
                }

                // PRESSED BACKSPACE
                case 127:
                {
                    // cmdmode에서는 줄넘김 안 쓰는 걸로 가정.
                    if (cmdmode && cp != clist->head)
                    {
                        cp = cp->prev;
                        list_remove(clist, cp->next);
                        print_behind_cursor(clist, cp, '\b', 0, 0);
                    }

                    // default (message) mode
                    else if (bp != blist->head)
                    {
                        // 지울 문자가 줄넘김일 때
                        if (bp->val == '\n')
                        {
                            int curpos = getCurposFromListptr(blist, bp->prev->prev);
                            eraseInputSpace(blist, bp);

                            bp = bp->prev->prev;
                            list_remove(blist, bp->next);
                            list_remove(blist, bp->next);

                            reprintList(blist, bp, curpos);
                        }

                        // 아닐 때 (단일 문자 삭제)
                        else
                        {
                            bp = bp->prev;
                            list_remove(blist, bp->next);
                            print_behind_cursor(blist, bp, '\b', 0, 0);
                        }
                    }

                    break;
                }

                // PRESSED DELETE
                case 128:
                {
                    // cmdmode에서는 줄넘김 안 쓰는 걸로 가정.
                    if (cmdmode && cp != clist->tail)
                    {
                        list_remove(clist, cp->next);
                        print_behind_cursor(clist, cp, 0, 0, 0);
                    }

                    // default (message) mode
                    else if (bp != blist->tail)
                    {
                        // 지울 문자가 줄넘김일 때
                        if (bp->next->val == '\r')
                        {
                            int curpos = getCurposFromListptr(blist, bp);
                            eraseInputSpace(blist, bp);

                            list_remove(blist, bp->next);
                            list_remove(blist, bp->next);

                            reprintList(blist, bp, curpos);
                        }
                        
                        // 아닐 때 (단일 문자 삭제)
                        else
                        {
                            list_remove(blist, bp->next);
                            print_behind_cursor(blist, bp, 0, 0, 0);
                        }
                    }
                    
                    break;
                }

                // PRESSED ENTER
                case 13:
                {
                    // 버퍼 리스트에 유의미한 데이터가 없으면 무시
                    if (list_is_empty(cmdmode ? clist : blist)) break;

                    if (cmdmode)
                    {
                        cp = transfer_list_data(cmd, clist, 1);

                        printf("\r\n");

                        // DO SOMETHING WITH THE COMMAND HERE...
                        // 명령어에 따른 동작 실행 e.g. 클라이언트 목록 뽑기

                        if (waiting_for_me)
                        {
                            waiting_for_me = 0;

                            if (cmd[0] == 'y' || cmd[0] == 'Y')
                            {
                                moveCursorUp(1, 1, 0);

                                send_singlechat_response(req_from, 1);

                                client_data[MEMBER_SRL].target = req_from;
                                client_data[req_from].target = MEMBER_SRL;
                                CHAT_STATUS = 1;
                                CHAT_TARGET = req_from;

                                printf("\033[1mAccepted chat request.\033[0m\r\n");

                                // "SERVER MESSAGE" 처럼 취급
                                printf("\r\n\n\n");
                                printf("\033[1;33m============ Started private chat! ============\033[0m");
                                printf("\r\n\n\n");
                                servmsg_printed = 1;

                                // 이제 채팅할 수 있다.
                                cmdmode = 0;
                                prompt_printed = 0;
                                break;
                            }

                            else
                            {
                                moveCursorUp(2, 1, 0);

                                send_singlechat_response(req_from, 0);

                                printf("\033[1mDeclined chat request.\033[0m\r\n> ");
                            }

                            fflush(stdout);
                        }

                        else if (!waiting_for_target)
                        {
                            //// 명령어에 따른 초기 동작 실행

                            // 숫자로 시작하면
                            if (cmd[0] > 47 && cmd[0] < 58)
                            {
                                if (strlen(cmd) > 2)
                                {
                                    moveCursorUp(2, 0, 0);
                                    printf("\033[1;33mNi de \033[4;7mCRAZY\033[0;1;33m ma?\033[0m\r\n> ");
                                    fflush(0);
                                    break; // continue;
                                }

                                req_to = atoi(cmd);

                                // ignore request to SELF
                                if (req_to == MEMBER_SRL)
                                {
                                    moveCursorUp(2, 0, 0);
                                    printf("\033[1;33m호구짓 하지 마쇼!!\033[0m\r\n> ");
                                    fflush(0);
                                    break; // continue;
                                }

                                if (client_data[req_to].chat_status > 0)
                                {
                                    moveCursorUp(2, 0, 0);
                                    printf("\033[1;33m해당 유저는 채팅중입니다\033[0m\r\n> ");
                                    fflush(0);
                                    break; // continue;
                                }

                                send_singlechat_request(req_to);

                                moveCursorUp(2, 1, 0);
                                printf("Requested chat with client %d. Waiting for response...\r\n", req_to);

                                waiting_for_server = 1;
                            }

                            // 'c'로 시작하면
                            else if (cmd[0] == 'c')
                            {
                                moveCursorUp(2, 1, 0);
                                printf("아직 구현 안 됨\r\n> ");
                            }

                            else
                            {
                                moveCursorUp(2, 1, 0);
                                printf("'%s' is not a valid command.\r\n> ", cmd);
                            }

                            fflush(stdout);
                        }
                    }

                    else
                    {
                        int lfcnt = getLFcnt(blist);
                        printf("\033[%dB", getLFcnt_from_node(bp, LIST_HEAD));

                        //
                        // AFTER THIS THE BUFFER LIST [WILL] BE EMPTY .
                        //
                        bp = transfer_list_data(buf, blist, 1);

                        //// SEND

                        // send_msg(OPENCHAT_CMD_CODE, buf);    // original
                        send_singlechat(buf);

                        //// READ (CREATE OUTPUT FROM SERVER MESSAGE)

                        // recv_msg(&cmdcode, sender, message);    // original

                        memset(buf, 0, BUF_SIZE);
                        read(sock, buf, BUF_SIZE);

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + lfcnt, 1, bp);
                        if (is_init) { printf("\r\n"); is_init = 0; }

                        if (servmsg_printed) servmsg_printed = 0;

                        // printf("\r\n%s sent: %s\r\n", sender, message); // original
                        printf("\r\n\033[1m%s\033[0m : %s\r\n", client_data[MEMBER_SRL].nick, &buf[CMDCODE_SIZE * 3]);

                        global_curpos = 0;

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
                        printf("\033[%dB", getLFcnt_from_node(bp, LIST_HEAD));
                        moveCursorUp(getLFcnt(blist), 1, 0);

                        list_insert(blist, bp, '\n');
                        list_insert(blist, bp, '\r');
                        bp = bp->next->next;

                        global_curpos = getLFcnt(blist);

                        reprintList(blist, bp, 0);
                    }

                    break;
                }

                // PRESSED OTHER: IS TYPING
                // 일반 입력 중
                default:
                {
                    // ALT + 키 중 사용 지정하지 않은 키값은 무시
                    if (c < 0) break;

                    if (cmdmode)
                    {
                        cp = list_insert(clist, cp, c);
                        print_behind_cursor(clist, cp, c, 0, 0);
                    }

                    else
                    {
                        // MESSAGE TOO LONG
                        if (blist->len > MSG_SIZE) break;

                        if (bp != blist->tail && bp->next->val == '\r') printf("%c", c);
                        else print_behind_cursor(blist, bp, c, 0, 0);
                        bp = list_insert(blist, bp, c);
                    }

                    break;
                }
            }

            fflush(stdout);
        }
    }
}
