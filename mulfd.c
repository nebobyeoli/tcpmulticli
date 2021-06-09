// From TCP SERVER
// CAN HANDLE MULTIPLE CLIENTS

// 컴파일 시 아래 복사해서 사용: 메인 소스 명시 후 추가 소스명 모두 입력
// gcc -o mulfd mulfd.c list/list.c list/list_node.c list/list_iterator.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <termios.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

/* 이중 연결 리스트 api
 * https://github.com/clibs/list
 */
#include "list/list.h"

//// 나중에 필요하다 싶으면 아래 변수들 함수들 헤더랑 따로 만들어서 담을 것

#define BUF_SIZE        1024 * 4    // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define CMDCODE_SIZE    4           // cmdcode의 크기
#define CMD_SIZE        20          // cmdmode에서의 명령어 최대 크기
#define NAME_SIZE       30          // 닉네임 최대 길이
#define MAX_SOCKS       100         // 최대 연결 가능 클라이언트 수

#define GETSERVMSG_TIMEOUT_SEC  0
#define GETSERVMSG_TIMEOUT_USEC 10000   // 1000000 usec = 1 sec
#define HEARTBEAT_INTERVAL      300       // HEARTBEAT 간격 (초 단위)

#define MIN_ERASE_LINES     1           // 각 출력 사이의 줄 간격 - 앞서 출력된 '입력 문구'를 포함하여, 다음 메시지 출력 전에 지울 줄 수
#define PP_LINE_SPACE       3           // 최솟값: 1  // 출력되는 메시지들과 '입력 문구' 사이 줄 간격

#define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))

int global_curpos = 0;

// 지정된 멀티캐스팅 주소
char mulcast_addr[] = "239.0.100.1";

int client[MAX_SOCKS];                  // 클라이언트 연결 저장 배열
char names[MAX_SOCKS][NAME_SIZE];       // 닉네임 저장 배열

// prompt-print message, 즉 '입력 문구'
char pp_message[] = "Input message(CTRL+C to quit):\r\n";

// command mode message, 즉 cmd mode에서의 입력 문구
char cmd_message[] = "Enter command(ESC to quit):\r\n> ";

// 'emoji' 문자열들 임시 내장 - 파일입출력으로의 사용 전까지

char myh[] = "⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀\r\n⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀\r\n⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀\r\n⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀\r\n⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀\r\n⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀\r\n⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀\r\n⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆\r\n⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦\r\n⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷\r\n⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹\r\n⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿\r\n⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷";

char bigbird[] = "                         .,,;!!=:-                \r\n                      ,-*@#@@@@@@$~               \r\n                     *$@@@@@@@@@@@@@~             \r\n                   !#@@@@@@@@@@@@@@@@@*.          \r\n                 -*#@@@@@@@@@@@@@@@@@@@=,        ~\r\n                .;@@!:#@@@@@@@@@@@@@@@@@#:   . ,#:\r\n               ,=@@=~,;@@@@@@@@@@@@@@@@@@@@@@@@@@.\r\n               @@@#~-- @#@@@@@@@@@@@@@@@@@@@@@@@= \r\n             .!@@@#::- =:=@@@@@@@@@@@@@@@@@@@@@@. \r\n             !@##-$*-- #::@@@@@@@@@@@@@@@@@@@@@$  \r\n             #-:@~*#, .#::~#@@@@@@@@@@@@@@@@@@@$  \r\n            :#: #*.-$==@$#!,@@@@@@@@@@@@@@@@@@@.  \r\n            $$~ *,. -=;.#@@@@@@@@@@@@@@@@@@@@@@   \r\n           .@#.,=-~; $;!#*,#@@@@@@@@@@@@@@@@@@;   \r\n           ,@@##$-;~ $-==!$.@@@@@@@@@@@@@@@@@@,   \r\n           ~@@$~!~-* #:.$!* #@@@@@@@@@@@@@@@@@,   \r\n           :@@#:::: !#$$;!-~##@@@@@@@@@@@@@@@@    \r\n           ~@@$,;@=$#;;!: $#~.$@@@@@@@@@@@@@@@.   \r\n           -@###*...@- ;*,-@-!-#@@@@@@@@@@@@@@,   \r\n           ,$~~$;,: ~##,*-.#!:-@@@@@@@@@@@@@@@    \r\n           ,#~~~;$$; ##$  :#@;#@@@@#@@@@@@@@@$    \r\n            @;!:*~~. !##=*#:-*@@@@@@@@@@@@@@@;    \r\n            #=~$!~ !-~@###!~~.#@@@@@@@@@@@@@@-    \r\n            !@-**;.=; $, #;=; $@@@@@@@@@@@@@#.    \r\n           -##~,$=;~; #;-**;; =@@@@@@@@@@@@@*     \r\n          =@@@;:#::., @;*!$-. $#@@@@@@@@@@@@.     \r\n        .#@@@@#$@;~,,,#--!@*;$@@@@@@@@@@@@@:      \r\n       ;@#@@@@@@@#~::$#@$@##@#@@@@@@@@@@@@$       \r\n     .###@@@@@@@@@@#@@#@@@@@@@@@@@@@@@@@@@;       \r\n    :@@@@*!;--,!@@@@@@##@@@@@@@@@@@@@@@@@*        \r\n  .:-           ,#@@@@@@@@@@@@@@@@@@@@@@:         \r\n          ,::$,!. $@@@@@@@@@@@@@@@@@@@$           \r\n         ;##@@@@@= *#@@@@@@@@@@@@@@@@-            \r\n        -$~=$#@@@@# .@@#@@@@@@@@@@@@@@-           \r\n        .!;;*$##@@@#=@: -:*@@@@$~.,*@@@;          \r\n         :,!,;:!,.@@@;      #@@@.   -#@@#~        \r\n         :.-       !         =@@@=    $@@*        \r\n         ;;-,                 *@@;     @@:        \r\n        ,.,~                 .@@#     -@@         \r\n        .. ,                 #@@      *@!         \r\n        ,. .         .      #@@       #@          \r\n        -  ..    ,@=## .   #@@.  -   -@@          \r\n        -  ,.   ,#$@@@=#  #@#.*!!@-- =@-          \r\n        ....   ,=~$#-=@@$@@#.,@@@@@@;@#           \r\n               - =* ~*.!;;;  #=#$.@@@@~           \r\n                !. .!       ::,# ;~               \r\n               .   :        - =  !                \r\n                             :  -                 \r\n                                .\r\n";

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
    printf("Length of buf: %d\r\n", (int)strlen(msg));
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

    int had_myh = 0;
    
    index = strstr(message, ":myh:");
    if (index)
    {
        inbet = (int)index - (int)message;
        
        memcpy(mdest, message, inbet);
        sprintf(&mdest[inbet], "\r\n%s\r\n%s", myh, &message[inbet + sizeof(":myh:") - 1]);

        had_myh = 1;
    }

    // 
    // message and mdest role swap time!
    // 

    index = strstr(had_myh ? mdest : message, ":bigbird:");
    
    if (index)
    {
        if (had_myh) {
            inbet = (int)index - (int)mdest;

            memcpy(message, mdest, inbet);
            sprintf(&message[inbet], "\r\n%s\r\n%s", bigbird, &mdest[inbet + sizeof(":bigbird:") - 1]);
            memcpy(mdest, message, BUF_SIZE);
        }
        
        else {
            inbet = (int)index - (int)message;

            memcpy(mdest, message, inbet);
            sprintf(&mdest[inbet], "\r\n%s\r\n%s", bigbird, &message[inbet + sizeof(":bigbird:") - 1]);
        }
    }

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
    else {
        (newnode->next = list_ptr->next)->prev = newnode;
        (newnode->prev = list_ptr)->next = newnode;
        list->len ++;
    }

    return newnode;
}

// https://stackoverflow.com/a/448982
/*
 * "기존 터미널 모드" 저장 변수
 * struct termios를 이용해 입력 방식을 바꿀 수 있다고 한다
 * 이를 이용해 줄넘김 없이, 타이핑 중에서의 직접 입력을 구현할 수 있다    와
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
    char c[7] = { 0, };

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

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <PORT>\n", argv[0]);
        exit(1);
    }
    
    time_t inittime;    // 서버가 시작된 시각
    time_t lasttime;    // 마지막 heartbeat 시각
    time_t now;         // 실시간 시각 (메인 반복문에서 매 순간 갱신)
    
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

    /* 실제 입력 관리: 이중 리스트 api 사용.
     * 엔터를 쳤을 때 blist 또는 clist의 데이터를 buf[]로 저장하고,
     * 해당 리스트를 초기화한다.
     */
    list_t *blist = list_new();     // buf_list
    list_node_t *bp;                // buf_list_pointer

    list_rpush(blist, list_node_new(0));    // blist->HEAD
    bp = blist->head;

    list_t *clist = list_new();     // cmd_list
    list_node_t *cp;                // cmd_list_pointer

    list_rpush(clist, list_node_new(0));    // clist->HEAD
    cp = clist->head;

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

    // 현재 포트를 address already in use 안 띄우고 재사용할 수 있게 한다 - setsockopt(SO_REUSEADDR)
    int on = 1;


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

    // 커널이 소켓의 포트를 점유 중인 상태에서도 서버 프로그램을 다시 구동할 수 있도록 한다
    // 즉 서버를 닫고 다시 열었을 때, 사용했던 포트를 재사용할 수 있도록 한다.
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    // 멀티캐스트 (char* join_addr) 주소에 가입
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

    inittime = lasttime = now = time(0);

    ////// CLIENT INTERACTION LOOP. //////

    while (1)
    {
        // REPRINT PROMPT ONLY ON OUTPUT OCCURENCE
        // 수신 메시지 존재 or 입력 버퍼 비워짐으로 추가 출력이 존재하는 경우에만 '입력 문구' 출력.
        if (!prompt_printed)
        {
            for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");

            printf("%s", cmdmode ? cmd_message : pp_message);

            if (cmdmode) reprintList(clist, cp, global_curpos);
            else reprintList(blist, bp, global_curpos);

            prompt_printed = 1;
        }

        // SEND HEARTBEAT REQUEST
        if ((now = time(0)) - lasttime >= HEARTBEAT_INTERVAL)
        {
            lasttime = now;

            if (cmdmode)
            {
                global_curpos = getCurposFromListptr(clist, cp);
                moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
            }

            else
            {
                global_curpos = getCurposFromListptr(blist, bp);
                moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + getLFcnt(blist), 1, bp);
            }

            //// sendAll에서와 좀 별도인 작동이 있어서 별도 반복문 수행

            int has_client = 0;

            for (i = 0; i < clnt_cnt; i++)
            {
                // DISCONNECT CLIENT에서 연결 해제되면 알아서 그만 보냄
                if (names[i][0] == 0) continue;

                if (!has_client) has_client = 1;
                write(client[i], "1500", CMDCODE_SIZE);
                printf("\r\n>> HEARTBEAT at [t: %ld] to   [%d] (%s)", (now = time(0)) - inittime, client[i], names[i]);
            }

            // 줄넘김 관리
            if (has_client) printf("\r\n");

            prompt_printed = 0;
	    }

        // kbhit() 내에서 select() 돌아감, 추가적 select() 필요 없음
        // 그래서 fd_set stdinfd 지움
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
                    reset_terminal_mode();

                    close(serv_sock);

                    if (cmdmode) moveCursorUp(PP_LINE_SPACE, 1, 0);
                    else         moveCursorUp(PP_LINE_SPACE + getLFcnt(blist), 1, bp);

                    printf("\r\nClosed server.");
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
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                        cmdmode = 0;
                    }

                    else
                    {
                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + getLFcnt(blist), 1, bp);
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

                        // 
                        // DO SOMETHING WITH THE COMMAND HERE...
                        // 명령어에 따른 동작 실행 e.g. 클라이언트 목록 뽑기
                        // cmd[some_index] ...
                        // 
                    }

                    else
                    {
                        int lfcnt = getLFcnt(blist);
                        printf("\033[%dB", getLFcnt_from_node(bp, LIST_HEAD));

                        //
                        // AFTER THIS THE BUFFER LIST [WILL] BE EMPTY .
                        //
                        bp = transfer_list_data(buf, blist, 1);

                        // CHECK FOR EMOJIS
                        char mdest[BUF_SIZE], umdest[] = "\r\nMESSAGE FROM SERVER:\r\n";
                        check_append_emojis(buf, mdest);
                        strcat(umdest, mdest);

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + lfcnt, 1, bp);

                        // SEND
                        if (mdest[0])   sendAll(clnt_cnt, 1000, serv_name, umdest, mdest);
                        else            sendAll(clnt_cnt, 1000, serv_name, buf, buf);

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
                        if (bp != blist->tail && bp->next->val == '\r') printf("%c", c);
                        else print_behind_cursor(blist, bp, c, 0, 0);
                        bp = list_insert(blist, bp, c);
                    }

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
            
            if (cmdmode) moveCursorUp(MIN_ERASE_LINES, 1, 0);
            else         moveCursorUp(MIN_ERASE_LINES + getLFcnt(blist), 1, bp);

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
                // 사실 여기서 비정상종료까지 다 체크되는 게 아닐까
                // 어쨌든 연결 끊겼으면 read()값도 없게 되는 게 아닐까
                if (read(client[i], buf, BUF_SIZE) <= 0)
                {
                    if (cmdmode) moveCursorUp(MIN_ERASE_LINES, 1, 0);
                    else         moveCursorUp(MIN_ERASE_LINES + getLFcnt(blist), 1, bp);

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
                        // SEND DISCONNECTED INFORMATION TO ALL CLIENTS
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

                    if (cmdcode != 1500)
                    {
                        if (cmdmode) moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                        else         moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + getLFcnt(blist), 1, bp);
                        
                        printf("\r\nReceived from [%d] (%s): %s %s\r\n", client[i], names[i], buf, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                    }

                    //
                    // 나중에 switch(cmdcode) 문으로 바꿀 것.
                    //

                    //// MODE : HEARTBEAT ////

                    if (cmdcode == 1500)
                    {
                        printf("<< HEARTBEAT at [t: %ld] from [%d] (%s)\r\n", (now = time(0)) - inittime, client[i], names[i]);
                    }

                    //// MODE : SET NAME ////
                    
                    else if (cmdcode == 2000)
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
                        // NEW BUFFER FOR USAGE AS PARAMETER FOR sendAll()
                        char msg[BUF_SIZE];
                        // EXTRACT MESSAGE FROM RECV BUFFER
                        strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE + 2]);
                        
                        // CHECK FOR EMOJIS
                        char mdest[BUF_SIZE];
                        check_append_emojis(msg, mdest);

                        // moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 0);

                        // SEND
                        if (mdest[0]) sendAll(clnt_cnt, 3000, names[i], mdest, mdest);
                        else          sendAll(clnt_cnt, 3000, names[i], msg, NULL);
                    }

                    //// FAILED TO IDENTIFY CMDCODE ////

                    else
                    {
                        printf("Error reading cmdcode from [%d]!\r\n", client[i]);
                    }
                }

                memset(buf, 0, BUF_SIZE);
                
                prompt_printed = 0;

                if (--state <= 0) break;
            }
        }
    }
}
