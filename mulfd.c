// From TCP SERVER
// CAN HANDLE MULTIPLE CLIENTS

// 컴파일 시 아래 복사해서 사용: 메인 소스 명시 후 추가 소스명 모두 입력
// gcc -o mulfd mulfd.c list/list.c list/list_node.c list/list_iterator.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

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

#define TTL  2

// 클라이언트 SRL이 시작되는 번호
// "Undefined cmdcode from 0 [0의 소켓]: 0"이 발생하는 이유를 확인하지 못함
// 위의 경우는 클라이언트 0이 서버와 다른 PC에서 접속했을 때 더 잘 발생하는 듯?
// 어디선가 EMPTY 0과 SRL 0을 구분하지 못해 발생하는 것으로 추측하고 있지만 확실하지는 않다.
#define FIRST_SRL           1

#define BUF_SIZE            1024 * 10   // 임시 크기(1024 * n): 수신 시작과 끝에 대한 cmdcode 추가 사용 >> MMS 수신 구현 전까지
#define MSG_SIZE            1000 * 10
#define CMDCODE_SIZE        4           // cmdcode의 크기
#define CMD_SIZE            20          // cmdmode에서의 명령어 최대 크기
#define NAME_SIZE           30          // 닉네임 최대 길이
#define MAX_SOCKS           100         // 최대 연결 가능 클라이언트 수
#define ACCEPT_MSG_SIZE     5           // 1 + sizeof(int)

#define EMOJI_VARIANT_MAX       30      // 최대 사용 가능 이모티콘 (txt) 수
#define EMOJI_TITLELEN_MAX      10      // 이모티콘(명령명) 최대 길이

#define GETSERVMSG_TIMEOUT_SEC  0
#define GETSERVMSG_TIMEOUT_USEC 10000   // 1000000 usec = 1 sec

#define UDP_INTERVAL            1       // UDP에 [서버 INFO] 제공할 시간 간격
#define HEARTBEAT_INTERVAL      3       // HEARTBEAT 간격 (초 단위)

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
int cmdmode = 0;

time_t inittime;    // 서버가 시작된 시각

time_t udp_lasttime;    // 마지막 heartbeat 시각
time_t udp_now;         // 실시간 시각 (메인 반복문에서 매 순간 갱신)

time_t lasttime;    // 마지막 heartbeat 시각
time_t now;         // 실시간 시각 (메인 반복문에서 매 순간 갱신)

// 지정된 멀티캐스팅 주소
char TEAM_MULCAST_ADDR[] = "239.0.100.1";

int client[MAX_SOCKS];              // 클라이언트 연결 저장 배열
char names[MAX_SOCKS][NAME_SIZE];   // 닉네임 저장 배열

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

char* dice_message[10] = {
    "\r\n뭐야 내 주사위 돌려줘요.\r\n나온 숫자 : ",
    "\r\n주사위는 잘못이 없습니다...\r\n나온 숫자 : ",
    "\r\n주사위 운도 실력입니다.\r\n나온 숫자 : ",
    "\r\n주사위는 최선을 다했습니다.\r\n나온 숫자 : ",
    "\r\n평균은 맞췄습니다.\r\n나온 숫자 : ",
    "\r\n예 저희가 주사위 많이 하죠.\r\n나온 숫자 : ",
    "\r\n꽤 높은 숫자입니다.\r\n나온 숫자 : ",
    "\r\n만족스럽네요.\r\n나온 숫자 : ",
    "\r\n오늘은 되는 날입니다.\r\n나온 숫자 : ",
    "\r\n떡상 가즈아ㅏㅏㅏ\r\n나온 숫자 : "
};

int emojiCnt = 0;

struct
{
    char title[EMOJI_TITLELEN_MAX];     // 이모티콘 파일명 저장
    long int len;                       // from long int ftell(FILE* stream)
} emojis[EMOJI_VARIANT_MAX];

struct sClient
{
    int logon_status;               // logon 되어있으면 1, 아니면 0
    char nick[NAME_SIZE];
    int chat_status;                // idle = 0, personal_chat = 1, channel_chat = 2
    int target;                     // 타겟 번호. 개인채팅이면 타겟 member_srl, 단체면 channel
    int is_chatting;                // 채팅 중인지
    time_t last_heartbeat_time;     // 마지막 heartbeat을 받은 시간

} client_data[MAX_SOCKS];           // member_srl은 client_data[i] 에서 i이다.

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
    perror(message);
    exit(0);
}

// integer to ascii
void itoa(int i, char *st)
{
    sprintf(st, "%d", i);
    return;
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

    // APPEND CMDCODE
    sprintf(message, "%d", cmdcode);
    // APPEND NAME OF SENDER
    sprintf(&message[CMDCODE_SIZE], "%s", sender);
    // APPEND MESSAGE
    sprintf(&message[CMDCODE_SIZE + NAME_SIZE], "%s", msg);
    
    if (servlog) printf("\r\n\033[1;34m>> \033[36m[TCP] \033[37mMESSAGE FROM SERVER: %s\033[0m\r\n", servlog);
    printf("Length of buf: %d\r\n", (int)strlen(msg));
    printf("Total msgsize: %d of %d maximum\r\n", CMDCODE_SIZE + NAME_SIZE + (int)strlen(msg), BUF_SIZE);

    for (i = FIRST_SRL; i < clnt_cnt; i++)
    {
        // DISCARD DISCONNECTED OR NAMELESS CLIENTS
        // 서버에서 나갔거나, 닉네임이 설정되지 않은 클라이언트는 무시한다.
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], message, BUF_SIZE);
        printf("\033[1;34mSent to client\033[0m %d [%d] (%s)\r\n", i, client[i], names[i]);
    }
}

void send_singlechat_request(int from, int to_sock)
{
    char pass[CMDCODE_SIZE * 2] = {0,};

    sprintf(pass, "%d", SINGLECHAT_REQ_CODE);
    sprintf(&pass[CMDCODE_SIZE], "%d", from);

    write(to_sock, pass, CMDCODE_SIZE * 2);
}

void send_singlechat_response(int from, int to_sock, int accepted)
{
    char pass[CMDCODE_SIZE * 3] = {0,};

    sprintf(pass, "%d", SINGLECHAT_RESP_CODE);
    sprintf(&pass[CMDCODE_SIZE], "%d", from);
    sprintf(&pass[CMDCODE_SIZE * 2], "%d", accepted);

    write(to_sock, pass, CMDCODE_SIZE * 3);
}

// singlechat <요청>에 대한 항목 추출
void extract_singlechat_response(char *buf, int *member_srl, int *accepted)
{
    char tmp[4] = {0,};

    memcpy(tmp, &buf[CMDCODE_SIZE * 1], 4);
    *member_srl = atoi(tmp);
    memcpy(tmp, &buf[CMDCODE_SIZE * 2], 4);
    *accepted = atoi(tmp);
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
    while (node = list_iterator_next(it)) if (node->val != 0 && node->prev->val == '\n') lfcnt++;
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
void check_append_emojis(char *msg, char *mdest)
{
    // EMOJI EMBEDDER
    // CURRENTLY SUPPORTS: EMBEDDED :myh: AND :bigbird:

    char message[MSG_SIZE];
    memcpy(message, msg, MSG_SIZE);
    memset(mdest, 0, MSG_SIZE);

    // TYPE *CHAR BECAUSE OF MSG TYPE,
    // BUT USED INT-CASTED FOR GETTING SIZE USING MEMORY LOCATION COMPARISONS
    char *index;

    // CAST TO INT FOR MEMORY LOCATION INDICATION
    // GET THE SIZE VALUE INBETWEEN
    unsigned long int inbet;

    // message and mdest ROLE SWAP
    int swap = 0;

    // emoji SWAP 로그 출력에서의 초기 줄넘김 삽입 위한
    int first = 1;

    // 이모티콘 txt 하나씩 읽음
    FILE *fp;

    // 실제 이모티콘을 dynamic array하게 저장
    char *tmp_emoji;

    // ./emojis/ 경로 내 저장된 이모티콘 파일의 수만큼 반복한다
    // 이 emojiCnt는 서버가 시작될 때 dirent를 이용해 도출되었다(사용 가능 emoji 정보 출력 부분).
    for (int i = 0; i < emojiCnt; i++)
    {
        // LENGTH of emoji DATA (2656, 1178, <...>)
        int LEN = emojis[i].len;

        // :myh:, :bigbird:, <:...:>
        char TOKEN[EMOJI_TITLELEN_MAX + 2 + 1] = ":";
        strcat(TOKEN, emojis[i].title);
        strcat(TOKEN, ":");

        // fopen()에 넣을 경로
        char cd[EMOJI_TITLELEN_MAX + 9 + 1] = "./emojis/";
        strcat(cd, emojis[i].title);
        strcat(cd, ".txt");

        // 연다
        fp = fopen(cd, "r");
        if (fp == NULL) { printf("Error opening file %s :(\r\n", cd); exit(1); }

        // 읽을 emoji.txt의 길이만큼 할당
        tmp_emoji = (char*)malloc((LEN + 1) * sizeof(char));
        if (tmp_emoji == NULL) { printf("malloc error on %s :(\r\n", cd); exit(1); }

        // emoji.txt의 데이터 대입
        size_t newLen = fread(tmp_emoji, sizeof(char), LEN, fp);
        if (ferror(fp) != 0) { printf("Error reading file %s :(\r\n", cd); exit(1); }
        else tmp_emoji[newLen] = 0;

        fclose(fp);

        // strstr는 문자열 내 문자열이 있는지 확인하고
        // 있으면 그 sub 문자열이 등장하는 시작 메모리 위치를, 없으면 NULL(= 0)을 반환한다.
        while (index = strstr(swap ? mdest : message, TOKEN))
        {
            if (first)
            {
                moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                printf("\r\n");
                first = 0;
            }

            // ㄴ은 니은이다
            printf("\r\n==============================================\r\n------------------------------------------\r\n%s\r\n\nㄴ==> Before swap", message);

            // message와 mdest 간 자리 교체 여부
            if (swap)
            {
                // sub 문자열과 원본 문자열 메모리 위치의 차를 이용해
                // 배열 index로 사용할 수 있는 시작 position을 구한다
                inbet = (long int)index - (long int)mdest;

                // 일단 복사한다
                memcpy(message, mdest, inbet);

                // 원본 배열 내, 아까 메모리 위치 차로 구한 position index 부터 실제 emoji를 삽입하고
                // 그 뒤에 :emoji: 뒤의 나머지 문자열을 추가 삽입한다
                // 나머지 문자열 추가 삽입은 :emoji:의 크기, 즉 strlen(TOKEN)을 이용해
                // 나머지 문자열의 시작 위치를 구해서 사용하면 된다
                memset(&message[inbet], 0, sizeof(&message[inbet]));
                sprintf(&message[inbet], "\r\n%s\r\n%s", tmp_emoji, &mdest[inbet + strlen(TOKEN)]);

                // 이제 다음 이모티콘 삽입에서의 역할 교환을 위해
                // 원본 배열에 emoji 삽입된 배열을 복사해 준다
                memcpy(mdest, message, MSG_SIZE);

                printf("\r\n------------------------------------------\r\n%s\r\n\nㄴ==> After swap\r\n------------------------------------------\r\ni = %d, swapped %s\r\n==============================================\r\n", message, i, TOKEN);

                swap = 0;
            }

            // 위와 같음, message와 mdest 자리 바뀜
            else
            {
                inbet = (long int)index - (long int)message;

                memcpy(mdest, message, inbet);
                memset(&mdest[inbet], 0, sizeof(&mdest[inbet]));
                sprintf(&mdest[inbet], "\r\n%s\r\n%s", tmp_emoji, &message[inbet + strlen(TOKEN)]);
                memcpy(message, mdest, MSG_SIZE);

                printf("\r\n------------------------------------------\r\n%s\r\n\nㄴ==> After swap\r\n------------------------------------------\r\ni = %d, swapped %s\r\n==============================================\r\n", message, i, TOKEN);

                swap = 1;
            }
        }

        // 공간 사용 후 비움
        memset(tmp_emoji, 0, LEN);
        free(tmp_emoji);
    }

    if (!first) for (int i = 0; i < MIN_ERASE_LINES + PP_LINE_SPACE; i++) printf("\r\n");

    fflush(stdout);
}

void check_append_Func(char *msg, char *mdest, int clnt_cnt)
{
    int randnum; // 랜덤변수
    char *index;
    char message[MSG_SIZE];
    char getNum[3];
    char pickMsg[] = "\r\n나만 아니면 돼!!!!!!\r\n이번 당번은.....  \033[7m ";

    while (index = strstr(msg, "/dice"))
    {
        unsigned int remaining_len = strlen(&index[5]);

        memcpy(index, &index[5], remaining_len);
        memset(&index[remaining_len], 0, remaining_len + 1);

        randnum = rand() % 100 + 1; // 0~99
        memset(getNum, 0, sizeof(getNum));
        itoa(randnum, getNum);

        strcpy(message, dice_message[(randnum-1) / 10]);
        strcat(message, "\033[7m ");
        strcat(message, getNum);
        strcat(message, " \033[27m");
        strcat(message, "\r\n");
        strcat(msg, message);

        memcpy(mdest, msg, MSG_SIZE);
    }

    if (index = strstr(msg, "/pickme")) // 픽미는 단일로 써야만 작동하게 할 것임. dice나 이모티콘이랑 섞이면 헷갈릴수 있음)
    {
        memset(message, 0, MSG_SIZE);

        unsigned int remaining_len = strlen(&index[7]);

        memcpy(index, &index[7], remaining_len);
        memset(&index[remaining_len], 0, remaining_len + 1);

        // 클라이언트 번호 랜덤 선택 후 반환
        while ((randnum = rand() % clnt_cnt) < FIRST_SRL);

        strcpy(message, pickMsg);
        strcat(message, names[randnum]);
        strcat(message, " \033[27m");
        strcat(msg, message);

        memcpy(mdest, msg, MSG_SIZE);
    }
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
    int lfcnt = getLFcnt_from_node(list_ptr, LIST_HEAD);
    if (lfcnt) printf("\033[%dB", lfcnt);

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

    memset(buf, 0, BUF_SIZE);

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

void disassembleHeartBeatPacket(char *message, struct HeartBeatPacket *hbp)
{
    char tmp[5] = {0,};
    int offset = 0;

    memcpy(&tmp, &message, sizeof(int));
    offset = sizeof(int);
    hbp->cmd_code = atoi(tmp);

    memcpy(&tmp, &message[offset], sizeof(int));
    offset += sizeof(int);
    hbp->member_srl = atoi(tmp);

    memcpy(&tmp, &message[offset], sizeof(int));
    offset += sizeof(int);
    hbp->chat_status = atoi(tmp);

    memcpy(&tmp, &message[offset], sizeof(int));
    offset += sizeof(int);
    hbp->target = atoi(tmp);

    memcpy(&tmp, &message[offset], sizeof(int));
    offset += sizeof(int);
    hbp->is_chatting = atoi(tmp);
}

// HeartBeat 패킷을 받았을 경우 실행
void HeartBeatProcess(char *message)
{
    struct HeartBeatPacket hbp;
    disassembleHeartBeatPacket(message, &hbp);

    time_t curr_time = time(NULL); // 현재 시간

    client_data[hbp.member_srl].logon_status = 1;
    client_data[hbp.member_srl].chat_status = hbp.chat_status;
    client_data[hbp.member_srl].target = hbp.target;
    client_data[hbp.member_srl].is_chatting = hbp.is_chatting;
    client_data[hbp.member_srl].last_heartbeat_time = curr_time;

    printf("HEARTBEAT! MS:%d, CS:%d, TG:%d, CT:%d\r\n", hbp.member_srl, hbp.chat_status, hbp.target, hbp.is_chatting);
}

// 해당 회원이 접속중인지 체크
void checkLogon(int clnt_cnt)
{
    time_t curr_time = time(NULL); // 현재 시간

    for (int i = FIRST_SRL; i < clnt_cnt; i++)
    {
        if (difftime(curr_time, client_data[i].last_heartbeat_time) > 10) // 마지막 heartbeat로부터 10초 이상 지났을 경우
        {
            client_data[i].logon_status = 0; // 로그온 안된거로 체크
        }
    }
}

// 클라이언트에게 리스트 시리얼화
void clientListSerialize(char *message, int clnt_cnt)
{
    char result[BUF_SIZE] = {0,};
    memset(&result, 0, BUF_SIZE);

    char tmp[5] = {0,};
    int offset = 0;

    for (int i = FIRST_SRL; i < clnt_cnt; i++)
    {
        // cmdcode: HEARTBEAT_STR_CODE
        memset(tmp, 0, sizeof(tmp));
        itoa(HEARTBEAT_STR_CODE, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);

        // member_srl: i
        memset(tmp, 0, sizeof(tmp));
        itoa(i, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);

        // nickname
        memcpy(&result[offset], &client_data[i].nick, sizeof(client_data[i].nick));
        offset += sizeof(client_data[i].nick);

        memset(tmp, 0, sizeof(tmp));
        itoa(client_data[i].logon_status, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);

        // chat_status
        memset(tmp, 0, sizeof(tmp));
        itoa(client_data[i].chat_status, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);

        // target
        memset(tmp, 0, sizeof(tmp));
        itoa(client_data[i].target, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);

        // is_chatting
        memset(tmp, 0, sizeof(tmp));
        itoa(client_data[i].is_chatting, tmp);
        memcpy(&result[offset], &tmp, sizeof(int));
        offset += sizeof(int);
    }

    memcpy(message, &result, BUF_SIZE); // 최종 메세지 저장
}

void memberlist_serialize_sendAll(int clnt_cnt)
{
    // HEARTBEAT_REQ_CODE 실행부처럼
    char send_message[BUF_SIZE] = {0,};
    clientListSerialize(send_message, clnt_cnt);

    for (int i = FIRST_SRL; i < clnt_cnt; i++)
    {
        if (client[i] < 0 || names[i][0] == 0) continue;

        write(client[i], send_message, BUF_SIZE);
        printf("\033[1;34m>>\033[0m MemberList \033[1;34mSent\033[0m      at [t: %ld] to   %d [%d] (%s)\r\n", (now = time(0)) - inittime, i, client[i], names[i]);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <IP OF PC> <PORT>\n", argv[0]);
        printf("IP OF PC: "); fflush(0);
        system("hostname -I");
        printf("\n"); fflush(0);
        exit(1);
    }

    // https://stackoverflow.com/a/34550798
    DIR *dirp = opendir("./emojis");
    static struct dirent *dir;

    int serv_sock, clnt_sock;
    int udp_sock;
    int time_live = TTL;

    int state = 0;              // 초기 연결 때의 오류 여부, 그 이후에는 클라이언트 상태 저장 용도로써 사용
    struct ip_mreq join_addr;   // 멀티캐스트 주소 저장
    struct sockaddr_in clnt_addr, serv_addr, mul_addr;

    struct timeval ti;          // timeval_(std)input - 입력값에 대한 타임아웃 구조체
    fd_set readfds, allfds;     // 클라이언트 배열 순회 용도, 둘의 정확한 차이나 용도는 불명

    int i, j;
    int clnt_cnt;
    int clnt_size;
    int fd_max;

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
     * 코드 진행 이후 '입력 문구'의 출력이 필요할 때는 prompt_printed = 1 실행
     */
    int prompt_printed = 0;

    /* sendAll()의 사용 형식에 맞추기 위한 '서버 이름'.
     * sendAll()에 사용할 때는 char *sender 인자에 넣으면 됨
     */
    char serv_name[NAME_SIZE] = "SERVER";

    // 현재 포트를 address already in use 안 띄우고 재사용할 수 있게 한다 - setsockopt(SO_REUSEADDR)
    int on = 1;

    srand(time(0)); // 랜덤시트 

    udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) perror_exit("UDP socket() error!\n");

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) perror_exit("TCP socket() error!\n");

    memset(&mul_addr, 0, sizeof(mul_addr));
    mul_addr.sin_family = AF_INET;
    mul_addr.sin_addr.s_addr = inet_addr(TEAM_MULCAST_ADDR);
    mul_addr.sin_port = htons(atoi(argv[2]));

    // UDP TTL
    setsockopt(udp_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&(time_live), sizeof(time_live));

    printf("\n\033[1;4;33mCONNECTED TO \033[36mUDP\033[33m MULTICAST.\033[0m\n");

    memset(&serv_addr, 0x00, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // TCP
    // 커널이 소켓의 포트를 점유 중인 상태에서도 서버 프로그램을 다시 구동할 수 있도록 한다
    // 즉 서버를 닫고 다시 열었을 때, 사용했던 포트를 재사용할 수 있도록 한다.
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    // TCP BIND
    state = bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (state == -1) perror_exit("TCP bind() error!\n");

    // TCP LISTEN
    state = listen(serv_sock, 5);
    if (state == -1) perror_exit("TCP listen() error!\n");

    ////// INITIALIZE //////

    memset(client, -1, sizeof(int) * MAX_SOCKS);

    // client_data 초기화
    for (int i = FIRST_SRL; i < MAX_SOCKS; i++)
    {
        memset(client_data[i].nick, 0, NAME_SIZE);
        client_data[i].target = -1;
    }

    /* 원본 iomux_select.c에서 clnt_cnt 조건 수정: 실제 개수로써 사용되도록.
     * 아래 클라이언트 반복문에서도 값 설정 때 i가 아닌 i + 1과 비교하고 대입하여 사용
     */
    clnt_cnt = -1;

    fd_max = serv_sock;

    for (int i = FIRST_SRL; i < MAX_SOCKS; i++) client[i] = -1;

    FD_ZERO(&readfds);
    // FD_SET(udp_sock, &readfds);
    FD_SET(serv_sock, &readfds);

    printf("\n\033[1;4;33mSTARTED \033[36mTCP\033[33m SERVER.\033[0m\n");

    // GET EMOJI NAMES.
    if (dirp)
    {
        FILE *fp;

        printf("\nUsable emojis (in ascending order):\n\n\033[1m");

        while ((dir = readdir(dirp)) != NULL)
        {
            if (strstr(dir->d_name, ".txt"))
            {
                memcpy(emojis[emojiCnt].title, dir->d_name, strlen(dir->d_name) - 4);

                printf("  - %s\n", emojis[emojiCnt].title);

                char cd[] = "./emojis/";
                strcat(cd, dir->d_name);

                fp = fopen(cd, "r");
                fseek(fp, 0L, SEEK_END);
                emojis[emojiCnt].len = (int)ftell(fp);
                fclose(fp);

                emojiCnt++;
            }
        }
        closedir(dirp);
    }

    printf("\033[0m\nemojiCnt: %d\n\n", emojiCnt);

    // for (int i = 1; i < PP_LINE_SPACE; i++) printf("\n");

    fflush(0);
    
    //// 이때부터 글자 하나씩 입력받기 모드 시작.
    set_conio_terminal_mode();

    memset(buf, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    memset(cmd, 0, CMD_SIZE);

    inittime = time(0);
    lasttime = now = inittime;
    udp_lasttime = udp_now = inittime;

    ////// CLIENT INTERACTION LOOP. //////

    while (1)
    {
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

        // SEND SERVER INFO TO [UDP SOCK]
        // 서버는 1초에 한 번씩 서버IP, 포트 정보 전송함
        if ((udp_now = time(0)) - udp_lasttime >= UDP_INTERVAL)
        {
            udp_lasttime = udp_now;

            if (cmdmode)
            {
                global_curpos = getCurposFromListptr(clist, cp);
                moveCursorUp(PP_LINE_SPACE, 1, 0);
            }

            else
            {
                global_curpos = getCurposFromListptr(blist, bp);
                moveCursorUp(PP_LINE_SPACE + getLFcnt(blist), 1, bp);
            }

            // sleep(1);

            char info[30] = {0,};
            memcpy(info, argv[2], sizeof(int));  // port
            memcpy(&info[sizeof(int)], argv[1], 20);  // pc ip

            sendto(udp_sock, info, strlen(info), 0, (struct sockaddr*)&mul_addr, sizeof(mul_addr));

            printf("\033[1;34m>> \033[36m[UDP]\033[34m SENT SERVER INFO\033[37m [%s]\033[0m at [t: %ld]\r\n", info, (now = time(0)) - inittime);
            // sleep(1);
            fflush(0);
            prompt_printed = 0;
        }

        // SEND HEARTBEAT REQUEST
        if ((now = time(0)) - lasttime >= HEARTBEAT_INTERVAL)
        {
            lasttime = now;

            //// sendAll에서와 좀 별도인 작동이 있어서 별도 반복문 수행

            int has_client = 0;

            // HEARTBEAT REQUEST MESSAGE
            memset(buf, 0, BUF_SIZE);
            sprintf(buf, "%d", HEARTBEAT_CMD_CODE);

            for (i = FIRST_SRL; i < clnt_cnt; i++)
            {
                // DISCONNECT CLIENT에서 연결 해제되면 알아서 그만 보냄
                if (names[i][0] == 0) continue;

                if (!has_client)
                {
                    has_client = 1;

                    if (cmdmode)
                    {
                        global_curpos = getCurposFromListptr(clist, cp);
                        moveCursorUp(PP_LINE_SPACE, 1, 0);
                    }

                    else
                    {
                        global_curpos = getCurposFromListptr(blist, bp);
                        moveCursorUp(PP_LINE_SPACE + getLFcnt(blist), 1, bp);
                    }
                }

                write(client[i], buf, BUF_SIZE);

                printf("\r\n\033[1;34m>> HEARTBEAT\033[0m at [t: %ld] to   %d [%d] (%s)\r\n", (now = time(0)) - inittime, i, client[i], names[i]);
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
                    // 서버가 종료되었다고 알림
                    memset(buf, 0, BUF_SIZE);
                    sprintf(buf, "%d", SERVCLOSED_CMD_CODE);
                    for (int i = FIRST_SRL; i < clnt_cnt; i++)
                    {
                        if (client[i] < 0 || names[i][0] == 0) continue;
                        write(client[i], buf, BUF_SIZE);
                        printf("\033[1;34mSent to client\033[0m %d [%d] (%s)\r\n", i, client[i], names[i]);
                    }

                    reset_terminal_mode();

                    close(serv_sock);

                    if (cmdmode) moveCursorUp(PP_LINE_SPACE, 1, 0);
                    else         moveCursorUp(PP_LINE_SPACE + getLFcnt(blist), 1, bp);

                    printf("\r\n\033[1;4;33mCLOSED SERVER.\033[0m");
                    for (int i = 0; i < PP_LINE_SPACE; i++) printf("\r\n");

                    return 0;
                }

                // PRESSED ESC
                case 27:
                {
                    if (cmdmode)
                    {
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

                            // sleep(1);

                            bp = bp->prev->prev;
                            list_remove(blist, bp->next);
                            list_remove(blist, bp->next);

                            // printf("\033[A");
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
                        printf("\033[2A\rCommand: %s\033[2B", cmd);

                        printf("\033[2K\r> ");
                        fflush(0);
                    }

                    else
                    {
                        int lfcnt = getLFcnt(blist);
                        printf("\033[%dB", getLFcnt_from_node(bp, LIST_HEAD));

                        //
                        // AFTER THIS THE BUFFER LIST [WILL] BE EMPTY .
                        //
                        bp = transfer_list_data(buf, blist, 1);

                        char mdest[MSG_SIZE], umdest[MSG_SIZE] = "\r\nMESSAGE FROM SERVER:\r\n";

                        // CHECK FOR EMOJIS
                        check_append_emojis(buf, mdest);

                        // CHECK FOR DICE
                        check_append_Func(mdest[0] ? mdest : buf, mdest, clnt_cnt);

                        memcpy(&umdest[strlen(umdest)], mdest, strlen(mdest));  // but also only while strlen(mdest) < MSG_SIZE - 24.
                        if (mdest[0]) strcat(umdest, "\r\n");

                        moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + lfcnt, 1, bp);

                        // SEND
                        if (mdest[0])   sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, umdest, mdest);
                        else            sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, buf, buf);

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

        if (cmdmode) global_curpos = getCurposFromListptr(clist, cp);
        else global_curpos = getCurposFromListptr(blist, bp);

        // ACCEPT CLIENT TO SERVER SOCK
        // 여기서 clnt_cnt는 증가하기만 한다. 감소하는 설정은 없다.
        if (FD_ISSET(serv_sock, &allfds))
        {
            clnt_size = sizeof(clnt_addr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_size);

            if (cmdmode) moveCursorUp(MIN_ERASE_LINES, 1, 0);
            else         moveCursorUp(MIN_ERASE_LINES + getLFcnt(blist), 1, bp);

            printf("\r\n\033[1;33m================================================\033[0m\r\n");
            printf("\033[1;33m<<\033[36m [TCP]\033[33m Connection from \033[37m(%s, %d)\033[0m\r\n", inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

            prompt_printed = 0;

            printf("\033[1m");
            for (i = FIRST_SRL; i < MAX_SOCKS; i++)
            {
                if (client[i] < 0)
                {
                    client[i] = clnt_sock;
                    printf("Client index : %d\r\n", i);
                    printf("Client FD    : %d\r\n", clnt_sock);
                    break;
                }
            }
            printf("\033[0m");

            printf("\033[1;33mAccepted \033[37m[%d]\033[0m\r\n", clnt_sock);
            printf("\033[1;33m================================================\033[0m\r\n");
            printf("\r\n");

            if (i == MAX_SOCKS) perror("\033[1m[Too many clients!]\033[0m\r\n");

            fflush(0);

            FD_SET(clnt_sock, &readfds);
            memset(names[i], 0, NAME_SIZE);

            if (clnt_sock > fd_max) fd_max = clnt_sock;
            if (i + 1 > clnt_cnt)   clnt_cnt = i + 1;
            if (--state <= 0)       continue;
        }

        // CHECK CLIENT SOCKET
        for (i = FIRST_SRL; i < clnt_cnt; i++)
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

                    printf("\033[1;33m================================================\033[0m\r\n");
                    printf("\033[1;33m-- \033[36m[TCP] \033[33mDisconnected client \033[37m%d [%d] (%s)\033[0m\r\n", i, client[i], names[i]);
                    printf("\033[1;33m================================================\033[0m\r\n");
                    printf("\r\n");
                    fflush(0);

                    close(client[i]);
                    FD_CLR(client[i], &readfds);
                    client[i] = -1;

                    // IF NAME WAS SET
                    // 연결 해제된 클라이언트의 이름을 지워 준다
                    if (names[i][0])
                    {
                        client_data[i].logon_status = 0; // 로그오프 처리
                        client_data[i].chat_status = 0;
                        client_data[i].is_chatting = 0;

                        memset(message, 0, BUF_SIZE);
                        sprintf(message, "\033[33m%s left the 광장. Goodbye, %s!", names[i], names[i]);

                        memset(names[i], 0, NAME_SIZE);
                        memset(client_data[i].nick, 0, NAME_SIZE);

                        client_data[client_data[i].target].target = -1;
                        client_data[i].target = -1;

                        // RESEND MEMBERLIST TO ALL CLIENTS
                        memberlist_serialize_sendAll(clnt_cnt);

                        // SEND DISCONNECTED INFORMATION TO ALL CLIENTS as SERVMSG
                        sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, message, message);
                    }
                }

                else
                {
                    char cmd_code[5] = {0,};
                    memcpy(&cmd_code, &buf, sizeof(int));
                    int cmdcode = atoi(cmd_code);

                    if (cmdcode != HEARTBEAT_CMD_CODE)
                    {
                        if (cmdmode) moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE, 1, 0);
                        else         moveCursorUp(MIN_ERASE_LINES + PP_LINE_SPACE + getLFcnt(blist), 1, bp);

                        int msgoffset;

                        switch (cmdcode)
                        {
                            case SINGLECHAT_RESP_CODE   : msgoffset = CMDCODE_SIZE * 2;         break;
                            case OPENCHAT_CMD_CODE      : msgoffset = CMDCODE_SIZE + NAME_SIZE; break;
                            case SINGLECHAT_CMD_CODE    : msgoffset = CMDCODE_SIZE * 3;         break;

                            default                     : msgoffset = CMDCODE_SIZE;             break;
                        }

                        printf("\r\n\033[1;33mReceived from\033[0m %d [%d] (%s): %d %s\r\n", i, client[i], names[i], cmdcode, &buf[msgoffset]);
                    }

                    switch (cmdcode)
                    {
                        //// MODE : HEARTBEAT RESPONSE from client ////
                        case HEARTBEAT_CMD_CODE:
                        {
                            printf("\033[A\033[1;35m<< HEARTBEAT\033[0m at [t: %ld] from\033[0m %d [%d] (%s)\r\n", (now = time(0)) - inittime, i, client[i], names[i]);

                            HeartBeatProcess(buf); // heartbeat 패킷 처리

                            break;
                        }

                        //// MODE : MEMBERLIST REQUEST from client ////
                        case HEARTBEAT_REQ_CODE:
                        {
                            printf("\033[1;35m<<\033[0m MemberList \033[1;35mRequested\033[0m at [t: %ld] from %d [%d] (%s)\r\n", (now = time(0)) - inittime, i, client[i], names[i]);

                            char send_message[BUF_SIZE] = {0,};
                            clientListSerialize(send_message, clnt_cnt);

                            write(client[i], send_message, BUF_SIZE);
                            printf("\033[1;34m>>\033[0m MemberList \033[1;34mSent\033[0m      at [t: %ld] to   %d [%d] (%s)\r\n", (now = time(0)) - inittime, i, client[i], names[i]);

                            break;
                        }

                        //// MODE : SINGLECHAT REQUEST from client ////
                        case SINGLECHAT_REQ_CODE:
                        {
                            int req_to = atoi(&buf[CMDCODE_SIZE]);

                            printf("\033[1;33mSinglechat Requested\033[0m from %d [%d] to %d [%d]\r\n", i, client[i], req_to, client[req_to]);

                            // '0'과 채팅한다 했을 때: [RETURN TO 광장]으로 취급
                            if (req_to == 0 && FIRST_SRL > 0)
                            {
                                send_singlechat_response(serv_sock, client[i], 1);

                                int orig_target = client_data[i].target;

                                client_data[orig_target].target = -1;
                                client_data[orig_target].chat_status = 0;

                                client_data[i].target = -1;
                                client_data[i].chat_status = 0;

                                memberlist_serialize_sendAll(clnt_cnt);

                                // Similar with:
                                // SEND JOIN INFORMATION TO ALL CLIENTS

                                memset(message, 0, BUF_SIZE);
                                sprintf(message, "\033[33m%s returned to the 광장!", names[i]);
                                sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, message, message);

                                memset(message, 0, BUF_SIZE);
                                sprintf(message, "\033[33m%s returned to the 광장!", names[orig_target]);
                                sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, message, message);
                            }

                            // 해당 클라이언트 연결이 없거나 이름 설정 안 되어 있음
                            if (client[req_to] == -1 || names[req_to][0] <= 0)
                            {
                                printf("\033[1;32m%d is not an existing client.\033[0m\r\n", req_to);
                                send_singlechat_response(serv_sock, client[i], 0);
                            }

                            else
                            {
                                send_singlechat_response(serv_sock, client[i], 1);
                                send_singlechat_request(i, client[req_to]);
                                printf("\033[1;32mRequested chat\033[0m to %d [%d].\r\n", req_to, client[req_to]);
                            }

                            break;
                        }

                        //// MODE : SINGLECHAT ACCEPT RESPONSE from client ////
                        case SINGLECHAT_RESP_CODE:
                        {
                            int resp_to, accepted;
                            extract_singlechat_response(buf, &resp_to, &accepted);
                            send_singlechat_response(i, client[resp_to], accepted);

                            if (accepted)
                            {
                                printf("\033[1;32mStarting private chat:\033[0m %d (%s) <==> %d (%s)\r\n", resp_to, names[resp_to], i, names[i]);

                                int orig_target = client_data[resp_to].target;

                                printf("\r\norig target: %d [%d] ==> %d [%d]\r\n", i, client[i], client_data[i].target, client[client_data[i].target]);
                                if (orig_target != -1) printf("\r\norig target: %d [%d] ==> %d [%d]\r\n", orig_target, client[orig_target], client_data[orig_target].target, client[client_data[orig_target].target]);
                                printf("\r\n");

                                // 원래 상대의 타켓 설정 해제
                                if (client_data[orig_target].target != -1)
                                {
                                    client_data[orig_target].target = -1;
                                    client_data[orig_target].chat_status = 0;

                                    // memset(message, 0, BUF_SIZE);
                                    // sprintf(message, "%d", SERVMSG_CMD_CODE);
                                    // sprintf(&message[CMDCODE_SIZE], "\033[35m%s left the private chat.", names[i]);

                                    // write(client[orig_target], message, BUF_SIZE);
                                }

                                client_data[i].target = resp_to;
                                client_data[i].chat_status = 1;

                                client_data[resp_to].target = i;
                                client_data[resp_to].chat_status = 1;

                                memberlist_serialize_sendAll(clnt_cnt);
                            }

                            else
                            {
                                printf("\033[1;32m%d (%s) declined chat with %d (%s).\033[0m\r\n", i, names[i], resp_to, names[resp_to]);
                            }

                            break;
                        }

                        //// MODE : REQUEST TO SET NAME from client ////
                        case SETNAME_CMD_CODE:
                        {
                            // CHECK IF REQUESTED NAME IS TAKEN
                            int taken = 0;
                            for (j = FIRST_SRL; !taken && j < clnt_cnt; j++)
                                if (!strcmp(&buf[CMDCODE_SIZE], names[j])) taken = 1;

                            //// REJECTED
                            if (taken)
                            {
                                printf("\033[1;32mName already taken:\033[0m %s\r\n", &buf[CMDCODE_SIZE]);
                                write(client[i], "0", ACCEPT_MSG_SIZE);
                            }

                            //// ACCEPTED
                            else
                            {
                                //// 고유 인덱스 i 보내 드림

                                char ACinfo[ACCEPT_MSG_SIZE] = {0,};
                                sprintf(ACinfo, "1");
                                sprintf(&ACinfo[1], "%d", i);

                                // 새 클라이언트에 i덱스 전달 완료
                                write(client[i], ACinfo, ACCEPT_MSG_SIZE);

                                memset(names[i], 0, NAME_SIZE);
                                memset(client_data[i].nick, 0, NAME_SIZE);
                                sprintf(names[i], "%s", &buf[CMDCODE_SIZE]);
                                sprintf(client_data[i].nick, "%s", &buf[CMDCODE_SIZE]);
                                printf("\033[1;32mSet name of client\033[0m %d [%d] as [%s]\r\n", i, client[i], names[i]);

                                // SEND JOIN INFORMATION TO ALL CLIENTS
                                memset(message, 0, BUF_SIZE);
                                sprintf(message, "\033[33m%s joined the 광장!", names[i]);

                                // 로그온 상태로 전환
                                client_data[i].logon_status = 1;

                                // 갱신된 클라이언트 memberlist 전송
                                memberlist_serialize_sendAll(clnt_cnt);

                                // ~ joined the chat!
                                sendAll(clnt_cnt, SERVMSG_CMD_CODE, serv_name, message, message);
                            }

                            break;
                        }

                        //// MODE : MESSAGE ////
                        case OPENCHAT_CMD_CODE:
                        case SINGLECHAT_CMD_CODE:
                        {
                            // NEW BUFFER FOR USAGE AS PARAMETER FOR sendAll()
                            // 이모티콘 삽입 위해, 추출된 실질 메시지 저장함
                            char msg[BUF_SIZE];

                            // EXTRACT MESSAGE FROM RECV BUFFER
                            if (cmdcode == OPENCHAT_CMD_CODE) strcpy(msg, &buf[CMDCODE_SIZE + NAME_SIZE]);
                            else if (cmdcode == SINGLECHAT_CMD_CODE) strcpy(msg, &buf[CMDCODE_SIZE * 3]);

                            char mdest[BUF_SIZE];

                            // CHECK FOR EMOJIS
                            check_append_emojis(msg, mdest);

                            // CHECK FOR DICE
                            check_append_Func(mdest[0] ? mdest : msg, mdest,clnt_cnt);

                            memset(message, 0, BUF_SIZE);

                            if (cmdcode == OPENCHAT_CMD_CODE)
                            {
                                // SEND RECEIVED MESSAGE TO ALL CLIENTS
                                if (mdest[0]) sendAll(clnt_cnt, OPENCHAT_CMD_CODE, names[i], mdest, mdest);
                                else          sendAll(clnt_cnt, OPENCHAT_CMD_CODE, names[i], msg, NULL);
                            }

                            else
                            {
                                if (mdest[0]) memcpy(&buf[CMDCODE_SIZE * 3], mdest, BUF_SIZE);

                                char tmp[5] = {0,};
                                memcpy(&tmp, &buf[CMDCODE_SIZE * 2], sizeof(int));
                                int target = atoi(tmp);

                                printf("\033[1;32mSinglechat:\033[0m %d [%d] ==> %d [%d]\r\n", i, client[i], target, client[target]);
                                fflush(stdout);

                                // 온 거 그대로 전달 <이모티콘 있으면 위 check_append_emojis에서 추가되었음>
                                write(client[i], buf, BUF_SIZE);
                                write(client[target], buf, BUF_SIZE);
                            }

                            break;
                        }

                        //// FAILED TO IDENTIFY CMDCODE ////
                        //// UNDEFINED CMDCODE FROM CLIENT ////
                        default:
                        {
                            printf("\033[1;31mUndefined\033[37m cmdcode from %d [%d]: %d\033[0m\r\n", i, client[i], cmdcode);

                            break;
                        }
                    }
                }

                memset(buf, 0, BUF_SIZE);

                prompt_printed = 0;

                if (--state <= 0) break;
            }
        }

        checkLogon(clnt_cnt); // 로그온 상태 갱신
    }
}
