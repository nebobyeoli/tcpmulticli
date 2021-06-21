# TCP multiple clients

#### `18WP 2021`

```c
// 지정된 멀티캐스팅 주소
char TEAM_MULCAST_ADDR[] = "239.0.100.1";
```

<!-- 단락 링크는 main으로 merge 시 제대로 작동되도록 작성함 -->

## 팀명

베리비안의 해적

## 수행 구성원

<!-- 아무리 생각해도 "중간개발자"는 말이 이상하단 말이야... -->

이름 | 직무 | 기록
---- | ---- | ----
양희수 | ![메인개발자](https://img.shields.io/badge/개발-메인개발자-blueviolet) ![author](https://img.shields.io/badge/관리자-author-ff5ecc) | ![완료보고서](https://img.shields.io/badge/문서-완료보고서-ff0048)
김영상 | ![메인개발자](https://img.shields.io/badge/개발-메인개발자-blueviolet) ![문서 총괄](https://img.shields.io/badge/관리자-문서%20총괄-ff5ecc) | ![요구사항명세서](https://img.shields.io/badge/문서-요구사항명세서-ff4000) ![기능정의서](https://img.shields.io/badge/문서-기능정의서-00bf36) ![설계서](https://img.shields.io/badge/문서-설계서-00cccc)
김창진 | ![서브개발자](https://img.shields.io/badge/개발-서브개발자-576aff) | ![설계서](https://img.shields.io/badge/문서-설계서-00cccc) ![notion](https://img.shields.io/badge/한눈에보기-notion-ff82b2)
김진우 | ![보조개발자](https://img.shields.io/badge/개발-보조개발자-8aa9ff) | ![시험명세서](https://img.shields.io/badge/문서-시험명세서-ccb25c)
이연호 | ![보조개발자](https://img.shields.io/badge/개발-보조개발자-8aa9ff) | ![시험명세서](https://img.shields.io/badge/문서-시험명세서-ccb25c)
정성원 | ![none](https://img.shields.io/badge/-none-b1b1b1) |
이용우 | ![none](https://img.shields.io/badge/-none-b1b1b1) |

직무 | 구현 항목
---- | --------
![메인개발자](https://img.shields.io/badge/개발-메인개발자-blueviolet) ![author](https://img.shields.io/badge/관리자-author-ff5ecc) | 입출력 체계화, UDP 송신, TCP 송수신, 이모티콘 사용, 색상 정리
![메인개발자](https://img.shields.io/badge/개발-메인개발자-blueviolet) ![문서 총괄](https://img.shields.io/badge/관리자-문서%20총괄-ff5ecc) | `HEARTBEAT` 체계화, `memberlist` 송수신, UDP 수신 및 TCP 가입
![서브개발자](https://img.shields.io/badge/개발-서브개발자-576aff) | `HEARTBEAT` 주기화, 메인 화면, 주사위, 제비뽑기
![보조개발자](https://img.shields.io/badge/개발-보조개발자-8aa9ff) | `HEARTBEAT` 주기화


<!--
<sub>문서 작업은 고단한 일입니다!</sub>

<sup><sup>제가 확실히 알지 못하는 코드 주석은 <sub>거의</sub> 작성하지 않습니다!</sup></sup>
-->

## Sources

Server source   | Client source | `list` source
--------------- | ------------- | -------------
[`mulfd.c (main branch)`](https://github.com/nebobyeoli/tcpmulticli/blob/main/mulfd.c) | [`mulcli.c (main branch)`](https://github.com/nebobyeoli/tcpmulticli/blob/main/mulcli.c) | [`clibs/list`](https://github.com/clibs/list)

## Compiling

```sh
gcc -o mulfd  mulfd.c  list/list.c list/list_node.c list/list_iterator.c
gcc -o mulcli mulcli.c list/list.c list/list_node.c list/list_iterator.c
```

## Usage

```sh
./mulfd  <IP OF PC> <PORT>
./mulcli <PORT>
```

## Main branches

Branch name | Pull request | Description
----------- | ------------ | -----------
**clistat** | [Manage Client Status](https://github.com/nebobyeoli/tcpmulticli/pull/2) | Server/Client 필요 기능 조건
**emojis**  | [Emoji support](https://github.com/nebobyeoli/tcpmulticli/pull/1) | 텍스트 이미지 이모티콘 및 긴 `mms` 송수신에 관하여
**keyinput**  | [Keyboard input by character](https://github.com/nebobyeoli/tcpmulticli/pull/4) | `termios`를 이용한 사용자 정의 `kbhit()` 및 `getch()` 활성화 <br> 글자를 하나씩 입력받아 직접 할당하는 방식으로의 입력 구현 <br> 즉 `엔터` 없이도, 입력 `중의` 입력 버퍼를 직접 관리할 수 있도록 하는 작업
**singles**  | [1 : 1 채팅 기반 구현](https://github.com/nebobyeoli/tcpmulticli/pull/11) | 개인 채팅 기반 구현

# 외부 작동

## 서버

### MESSAGE FROM SERVER

서버를 시작하면 <b>`STARTED TCP SERVER.`</b>의 문구와 함께 사용가능한 이모티콘 목록이 출력된다.

이때부터 서버는 TCP 소켓으로 메시지를 입력받아 전송할 수 있으며, 서버 메시지는 채팅 상대가 누구인지에 관계없이 닉네임과 통신 상대가 설정된 모든 클라이언트에게 전달된다.

서버는 1초에 한 번씩, UDP 멀티캐스트로 TCP 서버 소켓 가입 정보를 전송한다.

클라이언트는 멀티캐스트 그룹에 가입하고, 수신받은 서버 IP와 PORT 번호를 이용해 TCP 소켓에 접속할 수 있다.

### HEARTBEAT

서버는 닉네임이 설정된 클라이언트에 대해 일정 시간마다 클라이언트에 `HEARTBEAT`를 요청하고, 이에 응해 클라이언트가 전송한 `HEARTBEAT`를 출력한다. 이때 `HEARTBEAT`는 로그인 여부, 채팅중 여부, 누구랑 채팅중인지 등을 전송한다.

`HEARTBEAT` 요청을 받은 클라이언트는 `HEARTBEAT` 전송과 동시에 `memberlist`(클라이언트 목록)을 서버에 요청하고 수신받는다.

TCP 소켓에서 클라이언트와의 연결이 끊어지면, 서버는 해당 클라이언트를 정리한 후 나머지 클라이언트들에게 `%s left the 광장.`이라는 문구를 전송한다.

## 클라이언트 실행

### 접속

첫 실행 시 멀티캐스트 그룹에 가입하고 TCP 소켓 정보를 수신받는데 성공하면 아래와 같은 문구가 출력된다.

```sh
CONNECTED TO UDP MULTICAST.

<< RECEIVED    INFO MESSAGE.
<< SERVER IP : [IP OF PC]
<< PORT      : [TCP PORT]

CONNECTED TO TCP SERVER.


NICKNAME: 

```

### 닉네임 조건

- 닉네임은 공백 문자로 시작할 수 없다.
- 닉네임은 `NAME_SIZE`(30) 글자보다 길 수 없다.
- 모든 닉네임은 고유해야 한다. 이미 사용 중인 닉네임을 사용할 수 없다.

조건을 만족시키는 닉네임을 입력하면 아래와 같은 메인화면과 함께 채팅을 시작할 수 있게 된다.

## 클라이언트 통신

### 메인 메뉴

<details>
  <summary><b>메인 메뉴 출력 형태</b></summary><br>

```sh
MEMBER_SRL: 2
--------------------------------------------------------------
==================== Welcome To The 광장! ====================


NICKNAME: BBB

[Private chatting]

- 개인채팅 사용방법: 닉네임 앞 숫자 입력


==============================================================
--------------------------------------------------------------




============ BBB joined the 광장! ============






SRL 1 (AAA)
SRL 2 (BBB)
TOTAL NAMED CLIENTS: 2

Input message(CTRL+C to quit CHAT):

```
</details>

### 채팅 상대와 입력 모드 전환

#### [내부 작동](https://github.com/nebobyeoli/tcpmulticli/#개인-채팅)

#### 사용법

처음 접속한 후에는 `광장`이라는 이름의 오픈 채팅방에서 채팅이 시작된다.

여기서 전송되는 메시지는 `광장`에 나와 있는 모든 클라이언트에게 수신된다.

`ESC`를 눌러 `CMDMODE`와 `메시지 모드`를 전환할 수 있으며,<br>
`CMDMODE`에서 `SRL %d`로 표시된 클라이언트 고유번호를 입력해 그 클라이언트와의 개인채팅을 신청할 수 있다.

이때 자기 자신과, 이미 개인채팅 상대가 있는 클라이언트는 선택할 수 없고,<br>
개인채팅 도중에 채팅 상대가 없는 다른 클라이언트와의 개인채팅을 신청할 수 있으며,<br>
개인채팅 도중에 `CMDMODE`에 `0`을 입력하면 다시 `광장`으로 나갈 수 있다.

<details>
  <summary><b>개인 채팅 및 <code>광장</code>으로의 복귀 과정의 출력 형태</b></summary><br>

```sh
1 (AAA) is an existing client. Waiting for response...
1 (AAA) accepted the chat request.



============ Started private chat! ============



AAA : Hello!

CCC : Nice to meet you!

AAA : Goodbye!

CCC : Going back to the Gwangjang!




--------------------------------------------------------------
==================== Welcome To The 광장! ====================


NICKNAME: CCC

[Private chatting]

- 개인채팅 사용방법: 닉네임 앞 숫자 입력


==============================================================
--------------------------------------------------------------



============ Returned to the 광장! ============

============ AAA returned to the 광장! ============






SRL 1 (AAA)
SRL 2 (BBB)
SRL 3 (CCC)
TOTAL NAMED CLIENTS: 3

Input message(CTRL+C to quit CHAT):

```
</details>

### 채팅 상대 목록

```sh
SRL 1 (AAA)
SRL 2 (BBB)
SRL 3 (CCC)
TOTAL NAMED CLIENTS: 3
```

위와 같이 <b>`SRL [번호] (닉네임)`</b>으로 표시되는 클라이언트들의 상태는 색상으로 구분된다.

- 자기 자신은 굵은 초록색으로 표시된다.
- 채팅이 가능한, 즉 `광장`에 나와 있는 클라이언트는 굵은 흰색으로 표시된다.
- 다른 사람과 개인 채팅 중인 클라이언트는 얇은 자주색으로 표시된다.
- 자신과 개인 채팅 중인 클라이언트는 굵은 자주색으로 표시된다.

채팅 상대 목록은 새 접속, 접속 해제 상황을 포함하여 클라이언트 상태에 변화가 발생할 때마다 `memberlist`로 수신받아 실시간으로 갱신된다. 이는 `Input message(CTRL+C to quit CHAT)` 또는 `Enter command(ESC to exit CMDMODE)`의 입력 문구 바로 위에 고정 출력되어 다른 사용자들의 채팅 상태들을 편리하게 확인할 수 있다.

### 서버 메시지 수신

클라이언트에게 서버 메시지는 `============ [MESSAGE FROM SERVER] ============`의 형태로 출력된다.

- 사용자 접속 및 접속 해제와 같은 알림형 메시지는 굵은 노란색으로 표시된다.
- 서버 사용자가 직접 입력하여 전달되는 메시지는 굵은 흰색으로 표시된다.
- <details>
  <summary>사용자 입력값 중 부가기능(이모티콘, 주사위 등)이 삽입된 메시지는 다음과 같이 출력된다.</summary><br>

  서버에서의 입력값
  ```sh
  Input message(CTRL+C to quit CHAT):
  Hello this is a :myh: server
  ```
  
  클라이언트에서의 출력값
  ```sh
  ============ 
  MESSAGE FROM SERVER:
  Hello this is a 
  ⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀
  ⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀
  ⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀
  ⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀
  ⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀
  ⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀
  ⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀
  ⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆
  ⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦
  ⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷
  ⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹
  ⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿
  ⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷
  
   server
   ============
  ```
  </details>

## 서버 출력 로그

### 공통 출력

하트비트 및 멤버리스트 송수신 등의 시간 간격별 지속적 송수신은 아래와 같이 출력된다.

```sh
Received from 1 [3] (AAA): 1501 
<< MemberList Requested at [t: 90] from 1 [3] (AAA)
>> MemberList Sent      at [t: 90] to   1 [3] (AAA)

>> [UDP] SENT SERVER INFO [9000172.16.3.195] at [t: 91]

>> [UDP] SENT SERVER INFO [9000172.16.3.195] at [t: 92]

>> HEARTBEAT at [t: 93] to   1 [3] (AAA)
<< HEARTBEAT at [t: 93] from 1 [3] (AAA)
HEARTBEAT! MS:1, CS:0, TG:-1, CT:0
```

이와 같은 주기적 출력에서 공통되는 각 항목별 의미 및 자료형은 다음과 같다.

```c
// [t: n]:  Time since server start (n sec)     [LONG INT] (time(0) 반환형)
// 1     :  Client INDEX                        [INT]
// [3]   :  Client SOCKET                       [INT]
// (AAA) :  Client NAME                         [CHAR*]
```

### 색상 구분

굵은 색 | 출력 종류
------- | ---------
노랑 | 서버 시작/종료, 새로운 TCP 접속/접속 해제, TCP 수신 메시지 발생, 개인 채팅 신청 발생
초록 | 클라이언트 이름 설정, 개인 채팅 클라이언트 존재 여부
파랑 | `>>` 주기적 송신물
자주 | `<<` 주기적 수신물
시안 | `[UDP]` 송신, `[TCP]` 송신 및 연결 발생

## Cmd code 구분

<!-- Command code significations -->

기본은 천의 자리 수와 백의 자리 수로 구분하는 것을 원칙으로 한다.

Cmd code | Constant                | Meaning
-------- | ----------------------- | ---------------------
**1000** | `SERVMSG_CMD_CODE`      | **Message from server `서버 메시지`**
**1500** | `HEARTBEAT_CMD_CODE`    | **HEARTBEAT 송수신**
**1501** | `HEARTBEAT_REQ_CODE`    | **Memberlist request `클라이언트 리스트 요청`**
**1502** | `HEARTBEAT_STR_CODE`    | **Memberlist response `클라이언트 리스트 전송`**
  1600   | `SINGLECHAT_REQ_CODE`   | 개인 채팅 요청
  1601   | `SINGLECHAT_RESP_CODE`  | 개인 채팅 요청 응답
  2000   | `SETNAME_CMD_CODE`      | 클라이언트 닉네임 설정
**3000** | `OPENCHAT_CMD_CODE`     | **Messaging - Open chat `오픈채팅`**
**3001** | `SINGLECHAT_CMD_CODE`   | **Messaging - Single chat `개인채팅`**
  4000   | `SERVCLOSED_CMD_CODE`   | 서버가 종료되었음을 알림

## Message format

### 메시지 최대 크기

현재로서는 MMS로의 분할 수신 미구현으로, 송수신 메시지의 총 크기의 최댓값을 매우 큰 값으로 설정하였다.

Name              | _`[TOTAL OF ANY]`_
----------------- | ------------------
**Size constant** | `BUF_SIZE`
**Size**          | `1024 * n`

### 오픈채팅 & 서버 메시지 패킷

Name              | `cmdcode`      | `sender`    | `msg`
----------------- | -------------- | ----------- | ----------
**Size constant** | `CMDCODE_SIZE` | `NAME_SIZE` | `MSG_SIZE`
**Size**          | `4`            | `30`        | `1000 * n`

### 개인채팅 패킷

Name              | `cmdcode`      | `member_srl`  | `msg`
----------------- | -------------- | ------------- | ----------
**Size constant** | `CMDCODE_SIZE` | `sizeof(int)` | `MSG_SIZE`
**Size**          | `4`            | `4`           | `1000 * n`

### HEARTBEAT 패킷

<details>
  <summary><code>HeartBeatPacket</code> 구조체</summary><br>

```c
struct HeartBeatPacket
{
    int cmd_code;       // HEARTBEAT 전송 CMDCODE
    int member_srl;     // 클라이언트 고유번호
    int chat_status;    // 채팅 상대가 있는지 (idle = 0, personal_chat = 1, channel_chat = 2)
    int target;         // 채팅 상대
    int is_chatting;    // 타이핑 중인지
};
```
</details>

Name              | `cmd_code`      | `member_srl`  | `chat_status` | `target`      | `is_chatting`
----------------- | --------------- | ------------- | ------------- | ------------- | -------------
**Size constant** | `sizeof(int)`   | `sizeof(int)` | `sizeof(int)` | `sizeof(int)` | `sizeof(int)`
**Size**          | `4`             | `4`           | `4`           | `4`           | `4`


### 클라이언트 목록 `memberlist` 패킷

<details>
  <summary><code>sClient</code> 구조체</summary><br>

```c
struct sClient
{
    int logon_status;               // logon 되어있으면 1, 아니면 0
    char nick[NAME_SIZE];
    int chat_status;                // idle = 0, personal_chat = 1, channel_chat = 2
    int target;                     // 타겟 번호. 개인채팅이면 타겟 member_srl, 단체면 channel
    int is_chatting;                // 채팅 중인지
    time_t last_heartbeat_time;     // 마지막 heartbeat을 받은 시간

} client_data[MAX_SOCKS];           // member_srl은 client_data[i] 에서 i이다.
```
</details>

`client_data[]`의 배열로 저장하고, 송수신 때 하나의 `message`로 순차적으로 연결하여 사용한다.

Name              | `cmd_code`      | `logon_status`  | `nick`        | `chat_status` | `target`      | `is_chatting`
----------------- | --------------- | --------------- | ------------- | ------------- | ------------- | -------------
**Size constant** | `sizeof(int)`   | `sizeof(int)`   | `NAME_SIZE`   | `sizeof(int)` | `sizeof(int)` | `sizeof(int)`
**Size**          | `4`             | `4`             | `30`          | `4`           | `4`           | `4`

# 내부 작동

## 글자 하나씩 입력받기

기존의 배열 버퍼를 이용한 입력 사용은 메시지 수신으로 인한 `stdout`에서의 출력물 발생 시, 입력하던 내용이 입력 버퍼에 그대로 남아 있음에도 새로운 출력물로 인해 화면상에서 고정 출력으로 넘어가 더 이상의 수정 과정을 확인하지 못하는 문제가 있었다.

입력하던 내용의 재출력을 위해 아직 `ENTER`로 넘어가지 않은 입력 버퍼에 대한 접근 방법을 찾던 중, 리눅스에서의 터미널 입력 모드 전환을 이용한 `kbhit()` 및 `getch()`의 구현법을 알게 되었다. 이를 이용해, 글자를 하나씩 입력받아 각 값을 직접 저장해 주는 방식으로 입력 버퍼를 직접 관리하는 방법을 구현해 보게 되었다.

구현이 완료된 항목들은 다음과 같다.

- 출력 가능한 키 입력 발생 시 해당 문자를 이중 연결 리스트에 저장
- `좌/우 방향키` 및 `CTRL + 좌/우 방향키`를 이용해 한 글자 단위로 커서를 이동할 수 있음
- `BACKSPACE` 및 `DELETE`를 이용해 한 글자 단위로 입력 버퍼를 지울 수 있음
- `CTRL + BACKSPACE` 및 `CTRL + DELETE`를 이용해 한 블럭 단위로 입력 버퍼를 지울 수 있음
- `ALT + ENTER` 및 `BACKSPACE`와 `DELETE`를 이용해 입력 버퍼에 줄넘김을 삽입하고 삭제할 수 있음 (화면상 출력 버그가 발생하기는 하지만, 버퍼 리스트로 입력되는 내용은 온전함)

아래는 각 단계에서 Pull request를 통해 정리한 내용들을 한 바닥으로 합친 것이다.

### [1단계 구현 내용 기록](https://github.com/nebobyeoli/tcpmulticli/pull/4)

- [x] 글자를 하나씩 입력받도록 할 수 있는 '전제적 기반' 구현 완료

#### 구현 효과에 대한 예상

- **입력 버퍼에 직접 접근 가능**
- 키보드 입력 도중에 기타 키에 대한 `getch()`를 쓸 수 있음, `user가 입력 중입니다...`와 같은 입력 레코딩이 가능해질 수 있음
- **방향키 사용 가능**
- 복사한 내용을 붙여넣는다거나 한글 입력을 구현하려 할 때 매우 곤란해질 수도 있음
- 그러나 [codepoint to unicode 변환 함수](https://stackoverflow.com/a/38492214)를 참고해 유용히 구현해 내는 것이 가능할 수도 있음
- **매우 신기함!**

#### 관련해서 알게 된 것들

일반 입력키 외의 특수키들은 입력 발생 시 한 번에 여러 개의 정수값이 입력된다.<br>
즉 n번의 `getch()` 입력을 거쳐, 아래와 같은 값들을 반환한다.

Key     | `getch()` value(s)
------- | --------------------------------------------------------
`ESC`   | `27`
`UP`    | `27` `91`<sup>[</sup> `65`<sup>A</sup>
`DOWN`  | `27` `91`<sup>[</sup> `66`<sup>B</sup>
`RIGHT` | `27` `91`<sup>[</sup> `67`<sup>C</sup>
`LEFT`  | `27` `91`<sup>[</sup> `68`<sup>D</sup>
`DEL`   | `27` `91`<sup>[</sup> `51`<sup>3</sup> `126`<sup>~</sup>

### [2단계 구현 내용 기록](https://github.com/nebobyeoli/tcpmulticli/pull/5)

- [x] 수신 메시지 출력 후 입력 중 버퍼 재출력
- [x] 콘솔 창에서의 `엔터 키` 출력 이후에 대한 `BACKSPACE BEHAVIOR`
- [x] [이중 연결 리스트 사용](https://github.com/clibs/list)으로의 저장 방식 전환 구현

### [3단계 구현 내용 기록](https://github.com/nebobyeoli/tcpmulticli/pull/6)

- [x] `방향 키`로의 `커서 이동` 후 해당 위치에서의 문자 삽입에 대한 입력 버퍼 및 화면 출력값 동시 갱신
- [x] `BACKSPACE`, `DELETE` 키에 대한 작동 추가
- [x] `CTRL + 방향키` <sup>`LEFT`, `RIGHT` </sup>에 대한 작동 추가
- [x] `CTRL + ERASE` <sup>`BACKSPACE`, `DELETE` </sup>키에 대한 작동 추가

### [4단계 구현 내용 기록](https://github.com/nebobyeoli/tcpmulticli/pull/7)

#### 추가 구현

- [x] `공백`, `줄넘김`으로만 된 메시지는 `엔터` 날렸을 때 무시되도록
- [x] 출력물 출력 후에도, 화면에 표시된 사용자 입력물 유지

#### 해결한 버그

- [x] `줄넘김` 들어간 줄에 삽입할 때 `글자` 말고 `줄` 형식으로 `(같은 커서 위치에서 지속적으로)` 출력되도록
- [x] `세로로` 긴 <sup>엔터 많은</sup> 문자열 넘겼을 때도 최소 입력한 만큼은 커서 위로 올라가도록
- [x] 커서 뒤에 입력 데이터 존재하는 상황에서, 줄의 맨 끝에서 `ALT + ENTER` 넣었을 때 `줄넘김` 안 들어가는 현상
- [x] 줄넘김 앞, 줄의 끝에서 문자열 넣었을 때 `stdout`에 한 글자씩 출력되는 현상
- [x] `줄넘김` 앞에서 `오른쪽 화살표` 눌렀을 때 다음 줄로 커서 이동

### [5단계 구현 내용 기록](https://github.com/nebobyeoli/tcpmulticli/pull/9)

#### 키보드 입력

- [x] 클라이언트 코드에도 추가 구현된 입력 기법 적용
- [x] `ERASEKEY`와 `ALT + ENTER`의 연쇄 실행 시, 화면 출력 모양에서 가장 윗 줄이 그 다음 줄로 복사되어 전체 버퍼가 늘어나는 버그 발견

#### 기타

- [x] 타이핑 중의 여부를 함수로 작성
- [x] 닉네임이 설정된 직후의 클라이언트에게, 서버에서 그에게 할당된 `MEMBER_SRL`값을 송신해 주도록 구현

## 개인 채팅

개인 채팅에서는 위에서 설명된 [외부 작동](https://github.com/nebobyeoli/tcpmulticli/#채팅-상대와-입력-모드-전환)과 같이 원하는 상대에게 개인 채팅을 신청하고 이를 수락/거절할 수 있으며, 아래에 작성된 구현 완료 후 오픈 채팅과의 작동 결합으로 개인 채팅 상대 전환과 함께, 개인 채팅에서 오픈 채팅으로 되돌아 갈 수도 있도록 하는 구현을 완료하였다.

### [개인 채팅 구현 과정 기록](https://github.com/nebobyeoli/tcpmulticli/pull/11)

#### 개인 채팅 요청 및 수락/거절 과정

1. 개인 채팅을 하고자 하는 클라이언트 번호 (`target`) 입력
2. 해당 `target`이 존재하는지 서버에 확인 요청 및 응답 수신
3. 해당 `target`이 존재하면 서버가 `target` 클라이언트에게 개인채팅 요청 전달
4. 개인채팅 요청에 대한 답변을 서버가 원래 클라이언트에게 전달
5. 개인채팅 요청 수락 시, 성립된 클라이언트 관계를 `client_data[]`에 갱신한 후 모든 클라이언트에게 `memberlist`로 전달

#### 해결한 버그

- [x] 지속적으로 송신되는 `HEARTBEAT`에 의해 클라이언트에서 메시지 수신이 방해받는 현상
- [x] 서버가 개인 채팅 메시지 수신 후 메시지 수신자가 아닌 송신자로 다시 전송하던 현상
- [x] 서버 메시지를 수신만 하고 제대로 출력하지 못하는 현상: 메시지 분리 `offset` 길이가 잘못 사용되었음
- [x] 닉네임이 설정되지 않은 클라이언트에 개인 채팅을 요청할 수 없도록, 즉 '없는 클라이언트'처럼 취급하도록 구현함.

# 부가기능

## 이모티콘 사용

메시지 모드에서 `:이모티콘파일명:`을 입력하면, 서버에서 `:이모티콘파일명:`들을 각 토큰에 대응하는 이모티콘 파일의 내용으로 교환하여 수신자에게 전송한다. 구현은 `strstr()`의 메모리 위치 반환값의 차를 이용하여 인덱스를 구하고, 이를 기준으로 배열을 분할하여 중간 삽입을 수행하는 방식을 사용하였다.

### 이모티콘 추가법

- 서버 PC에서 `./mulfd`를 실행하기 전, 사용하고자 하는 이모티콘을 `./emojis/`에 `txt` 파일로 추가한다.
- 이모티콘 `txt`파일은 하나의 개행 문자로 끝나도록 한다.
- 파일명은 해당 이모티콘에 사용하고자 하는 **10자 이내의** 명령어로 한다.

### 주의사항

- 서버 실행 도중에 이모티콘 파일을 지우면 파일 읽기 오류로 서버가 종료된다.
- 서버 실행 도중에 이모티콘 파일 내용을 수정하면 해당 이모티콘의 다음 사용 때부터 수정된 이모티콘으로 사용된다.
- **MMS 분할 미구현으로, 한 번에 너무 많은 이모티콘을 보내게 되면 최대 메시지 크기 초과로 Segmentation fault 등이 발생할 수 있다.**

### 사용 예시

Usage     | File               | Appearance
--------- | ------------------ | ----------
**:myh:** | `./emojis/myh.txt` | ⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀<br>⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀<br>⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀<br>⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀<br>⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀<br>⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀<br>⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀<br>⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆<br>⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦<br>⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷<br>⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹<br>⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿<br>⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷<br>`[\n]` |
**:face:** | `./emojis/face.txt` | `[\n]`<br>( ͡° ͜ʖ ͡°)( ͠° ͟ʖ ͡°)( ͡~ ͜ʖ ͡°)( ͡ʘ ͜ʖ ͡ʘ)( ͡o ͜ʖ ͡o)<br>`[\n]`
<!-- **:bbird:** | `./emojis/bbird.txt` |  -->

## 주사위

메시지 모드에서 `/dice`를 보내면 랜덤으로 1~100까지의 수 중 하나의 수를 선택한 후에 그 숫자의 범위에 따라 다음 문구가 출력되도록 하였다.

```c
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
```

### 사용 예

입력 예시
```sh
Input message(CTRL+C to quit):
/dice/dice/dice 
```

출력 예시
```sh
예 저희가 주사위 많이 하죠.
나온 숫자 : 58

만족스럽네요.
나온 숫자 : 76

주사위는 잘못이 없습니다...
나온 숫자 : 14
```

## 제비뽑기

메시지 모드에서 `/pickme`를 입력하면 서버에서 범위 내의 클라이언트 번호 중 하나를 랜덤으로 선택하고 그의 닉네임을 전송한다.

### 사용 예

입력 예시
```sh
Input message(CTRL+C to quit):
/pickme
```

출력 예시
```sh
나만 아니면 돼!!!!!!
이번 당번은.....    : [랜덤 닉네임]
```

# 그 외

평소 당연하다고 여기는 출력 모양을 하나하나 직접 구현해 낸 부분이 가장 까다로우면서도 도전적이었던 것 같다.

## Notes to self.

출처가 있는 항목들은 가져온 코드들이고 출처가 없는 항목들은 직접 작성한 코드 내용이다

### 소스 중 특정 일부씩 참고용

###

<details>
  <summary><b>About custom function <i>moveCursorColumnblock()</i></b></summary><br>

  ```c
  // 4개 케이스를 하나로 줄이는 과정에서 좀 <헷갈리게> 생겨먹어지게 되었기 때문에
  // 원래 switch-case문에서 작성한 코드를
  // 삽입하는 바 이 다
  // 안 그러면 아무래도 어쩌다 <내가> 나중에 망할 것 같아서
  // 그 렇 다
  ```

  ```c
  // 각각의 moveCursorColumnblock()과 바로 아래 2~3줄의 코드는 같은 역할을 수행한다
  // 요 아래 과정들을 한 함수로 뽀개 넣으면 일단 중복 코드가 3줄 x 8개나 있던 것이 확 줄어드는 장점이 있다
  // 실제 [CTRL + ERASEKEY] 작동들 더 들여다보면서 실 코드에서의 반복 조건들은 좀 바뀔 수 있음
  ```

  <details>
    <summary><b>원래 썼던 switch/case 내용</b></summary><br>
  
  ```c
  // CTRL + LEFT ARROW [←]
  case -20:
  {
      if (cmdmode)
      {
          // cp = moveCursorColumnblock(clist, cp, "\b", 0, LIST_HEAD);
          for (         ; cp != clist->head && cp->val != '\n' && cp->val == ' '; printf("\b"), cp = cp->prev, i++);
          if  (!i) for (; cp != clist->head && cp->val != '\n' && cp->val != ' '; printf("\b"), cp = cp->prev, i++);
      }
  
      else
      {
          // bp = moveCursorColumnblock(blist, bp, "\b", 0, LIST_HEAD);
          for (         ; bp != blist->head && bp->val != '\n' && bp->val == ' '; printf("\b"), bp = bp->prev, i++);
          if  (!i) for (; bp != blist->head && bp->val != '\n' && bp->val != ' '; printf("\b"), bp = bp->prev, i++);
      }
  
      break;
  }
  
  // CTRL + RIGHT ARROW [→]
  case -19:
  {
      if (cmdmode)
      {
          // cp = moveCursorColumnblock(clist, cp, "\033[C", 0, LIST_TAIL);
          for (         ; cp != clist->tail && cp->next->val != '\n' && cp->next->val != ' '; printf("\033[C"), cp = cp->next, i++);
          if  (!i) for (; cp != clist->tail && cp->next->val != '\n' && cp->next->val == ' '; printf("\033[C"), cp = cp->next, i++);
      }
  
      else
      {
          // bp = moveCursorColumnblock(blist, bp, "\033[C", 0, LIST_TAIL);
          for (         ; bp != blist->tail && bp->next->val != '\n' && bp->next->val != ' '; printf("\033[C"), bp = bp->next, i++);
          if  (!i) for (; bp != blist->tail && bp->next->val != '\n' && bp->next->val == ' '; printf("\033[C"), bp = bp->next, i++);
      }
  
      break;
  }
  
  // PRESSED CTRL + BACKSPACE
  case 8:
  {
      if (cmdmode)
      {
          // cp = moveCursorColumnblock(clist, cp, "\b", 1, LIST_HEAD);
          for (         ; cp != clist->head && cp->val != '\n' && cp->val == ' '; printf("\b"), cp = cp->prev, list_remove(clist, cp->next), i++);
          if  (!i) for (; cp != clist->head && cp->val != '\n' && cp->val != ' '; printf("\b"), cp = cp->prev, list_remove(clist, cp->next), i++);
          print_behind_cursor(clist, cp, 0, ' ', i);
      }
  
      else
      {
          // bp = moveCursorColumnblock(blist, bp, "\b", 1, LIST_HEAD);
          for (         ; bp != blist->head && bp->val != '\n' && bp->val == ' '; printf("\b"), bp = bp->prev, list_remove(blist, bp->next), i++);
          if  (!i) for (; bp != blist->head && bp->val != '\n' && bp->val != ' '; printf("\b"), bp = bp->prev, list_remove(blist, bp->next), i++);
          print_behind_cursor(blist, bp, 0, ' ', i);
      }
      
      break;
  }
  
  // PRESSED CTRL + DELETE
  case -128:
  {
      if (cmdmode)
      {
          // moveCursorColumnblock(clist, cp, 0, 1, LIST_TAIL);
          for (         ; cp != clist->tail && cp->next->val != '\n' && cp->next->val == ' '; list_remove(clist, cp->next), i++);
          if  (!i) for (; cp != clist->tail && cp->next->val != '\n' && cp->next->val != ' '; list_remove(clist, cp->next), i++);
          print_behind_cursor(clist, cp, 0, ' ', i);
      }
  
      else
      {
          // moveCursorColumnblock(blist, bp, 0, 1, LIST_TAIL);
          for (         ; bp != blist->tail && bp->next->val != '\n' && bp->next->val == ' '; list_remove(blist, bp->next), i++);
          if  (!i) for (; bp != blist->tail && bp->next->val != '\n' && bp->next->val != ' '; list_remove(blist, bp->next), i++);
          print_behind_cursor(blist, bp, 0, ' ', i);
      }
      
      break;
  }

  ```
  </details>
  
  <details>
    <summary><b>단일 함수로 <sup>반복 사용부! 가</sup> 축약된 내용</b></summary><br>
  
  ```c
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
   * int dirTo:
   *   리스트의 dirTo까지 확인
   *   0: LIST_HEAD: 리스트의 head 방향으로 반복문 돌아감
   *   1: LIST_TAIL: 리스트의 tail 방향으로 반복문 돌아감
   * 
   * tp != (dirTo ? list->tail : list->head):
   * 원래 list_node_t *end = (dirTo ? list->tail : list->head)로 초기조건 변수를 만들어 저장하려 했으나
   * 각 반복문이 매 바퀴를 돌 때마다 list->tail값이 변화되기 때문에
   * end가 가리킬 위치를 매번 갱신해 주지 않으면, 줄의 맨 끝에서 [CTRL + DELETE]를 눌렀을 때
   * Segmentation fault (core dumped)가 발생한다.
   */
  list_node_t* moveCursorColumnblock(list_t *list, list_node_t *p, char *printstr, int modifying, int dirTo)
  {
      int i = 0;
      list_node_t *tp = p;
  
      while (tp != (dirTo ? list->tail : list->head) && (dirTo ? tp->next : tp)->val != '\n' && (dirTo ? tp->next->val != ' ' : tp->val == ' '))
      {
          if (printstr) { printf("%s", printstr); tp = (dirTo ? tp->next : tp->prev); }
          if (modifying) list_remove(list, tp->next);
          i++;
      }
  
      if (!i) while (tp != (dirTo ? list->tail : list->head) && (dirTo ? tp->next : tp)->val != '\n' && (dirTo ? tp->next->val == ' ' : tp->val != ' '))
      {
          if (printstr) { printf("%s", printstr); tp = (dirTo ? tp->next : tp->prev); }
          if (modifying) list_remove(list, tp->next);
          i++;
      }
  
      if (modifying) print_behind_cursor(list, tp, 0, ' ', i);
  
      return tp;
  }

  ```
  </details>

</details>

<details>
  <summary>(stashed - 만들어 놓고 보니 사용 안 해서 다시 지움)</summary><br>

  ```c
  // mulfd.c

  // singlechat <메시지>에 대한 항목 추출
  // char *chat_msg:  추출된 메시지 저장 배열,
  // char *message:   read() 받은 메시지 자체
  void disassembleSinglechat(int *target_srl, char *chat_msg, char *message)
  {
      char tmp[5]={0,};
      int offset = sizeof(int);
  
      memcpy(&tmp, &message[offset], sizeof(int));
      offset += sizeof(int);
      *target_srl = atoi(tmp);
  
      memcpy(chat_msg, &message[offset], MSG_SIZE);
  }

  ```
</details>

##

### 새로 알게 된 것들

<details>
  <summary><b>Debug logging without altering output to stdout</b></summary><br>

  ```c
  // 사용 중의 stdout 콘솔창 대신 대신 별도 외부 파일에 확인용 출력 출력
  // 출력 형식에 변동이 가지 않아 값 확인용 디버깅 때에 편리하다.
  FILE* fp = fopen("log.log", "a+");
  fprintf(fp, "%d\n", lfcnt);
  fclose(fp);

  ```
  
  ###

  ```c
  // log.log에로의 실시간 변화 데이터는 아래와 같이 확인할 수 있다.
  ```

  ```sh
  tail -f log.log
  ```
</details>

<details>
  <summary><b>Cursor manipulation (go <i>up</i>) in linux</b></summary><br>
  <!-- syntax highlighting breaks on tabsize = 4, unfortunately -->

  ```c
  // CURSOR POSITION MANIPULATION
  // FOR LINUX SYSTEMS

  #include <stdio.h>

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

  int main(void)
  {
      printf("1111111111111111111111111111111111\n");
      printf("2222222222222222222222222222222222\n");
      printf("3333333333333333333333333333333333\n");
      printf("4444444444444444444444444444444444\n");
      printf("5555555555555555555555555555555555");

      moveCursorUp(2, 0);

      printf("-boop-");

      return 0;
  }

  ```
</details>

<details>
  <summary><b>Cursor manipulation <i>(gotoxy)</i> in linux</b></summary><br>

  ```c
  // Question: How to position the input text cursor in C?
  // https://stackoverflow.com/questions/26423537/how-to-position-the-input-text-cursor-in-c
  ```

  ```c
  // Answer 1: https://stackoverflow.com/a/26423946 comment below

  // gotoxy(x, y) linux
  #define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))

  ```

  ```c
  // Answer 2: https://stackoverflow.com/a/26423857
  // 주의: X에 0이 들어가도 최소 하나 출력된다.
  // 이것 때문에 예상치 못한 곳에서 자주 당했다 .

  // In the linux terminal you may use terminal commands to move your cursor, such as
  printf("\033[8;5Hhello");   // Move to (8, 5) and output hello

  // other similar commands:
  printf("\033[XA");  // Move up X lines;
  printf("\033[XB");  // Move down X lines;
  printf("\033[XC");  // Move right X columns;
  printf("\033[XD");  // Move left X columns;
  printf("\033[2J");  // Clear screen

  // 화살표 키입력과 같이 위, 아래, 오른쪽, 왼쪽을 각각 A, B, C, D로 기억하면 편하다.

  ```
</details>

<details>
  <summary><b>Mimicking str.replace functioning behavior in C</b></summary><br>

  ```c
  // IMPLEMENT SUBSTRING REPLACEMENT

  #include <stdio.h>
  #include <string.h>

  #define BUF_SIZE 1024

  int main(void)
  {
      char st[] = "The quick brown fox :uyaho: jumps over the lazy dog";
      printf("%d\n", sizeof(st));
      
      char ss[BUF_SIZE];
      char *index = strstr(st, ":uyaho:");
      
      // CAST TO INT FOR MEMORY LOCATION INDICATION
      // GET THE SIZE VALUE INBETWEEN
      int inbet = (int)index - (int)buf;

      memcpy(ss, st, inbet);
      sprintf(&ss[inbet], "<token>");
      sprintf(&ss[inbet + sizeof("<token>") - 1], &st[inbet + sizeof("<token>") - 1]);
      
      printf("%s\n%s\n", st, ss);

      // bonus?
      printf("size: %ld, length: %ld\n", sizeof(ss), strlen(ss));
      
      return 0;
  }

  ```
</details>

<details>
  <summary><b>sprintf는 sprintf할 문자열 이후에 NULL 문자만 하나 넣고 그 뒤 데이터를 초기화해 주지 않는다</b></summary><br>

  ```c
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  
  // integer to ascii
  void itoa(int i, char *st)
  {
      sprintf(st, "%d", i);
      return;
  }
  
  int main(void)
  {
      char st[] = "The big brown fox";
      printf("%s %ld\n", st, sizeof(st));
  
      memset(st, 0, sizeof(st));
      itoa(50, st);
      sprintf(st, "50");
  
      printf("%s %ld\n", st, sizeof(st));
  
      printf("Data:\n");
      for (int i = 0; i < sizeof(st); i++) printf("%d\t", st[i]);
      printf("\n");
      for (int i = 0; i < sizeof(st); i++) printf("%c\t", st[i]);
      printf("\n");
  
      return 0;
  }
  
  ```
</details>

<details>
  <summary><b>Implementing kbhit() and getch() in LINUX C</b></summary><br>

  ```c
  // 리눅스에서 C언어로 getch() 함수 작성
  // conio.h는 C 표준 라이브러리에 속하지 않아 기본으로는 리눅스에서 지원하지 않는다.
  // 따라서 아래느님들처럼 termios.h를 이용해 터미널 모드를 바꿔 엔터 키 없이도 입력값을 각각 읽을 수 있도록 한다.
  // 아래와 같은 터미널 모드 변경 시에는 linefeed를 \n 대신 \r\n와 같이 사용해야 한다.

  // 코드들 찾기 대빡나게 힘들었다.
  ```

  <details>
    <summary><b>#1.</b> getch() 함수만, 터미널 설정을 한 함수에 내장</summary><br>

  ```c
  // 첫 번째는 터미널 설정을 한 함수에 내장, getch() 함수만

  // https://lunace.tistory.com/6 코드

  #include <stdio.h>
  #include <string.h>
  #include <termios.h>
  #include <unistd.h>

  int getch()
  {
      int c;
      struct termios oldattr, newattr;

      tcgetattr(STDIN_FILENO, &oldattr);           // 현재 터미널 설정 읽음
      newattr = oldattr;
      newattr.c_lflag &= ~(ICANON | ECHO);         // CANONICAL과 ECHO 끔
      newattr.c_cc[VMIN] = 1;                      // 최소 입력 문자 수를 1로 설정
      newattr.c_cc[VTIME] = 0;                     // 최소 읽기 대기 시간을 0으로 설정
      tcsetattr(STDIN_FILENO, TCSANOW, &newattr);  // 터미널에 설정 입력
      c = getchar();                               // 키보드 입력 읽음
      tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);  // 원래의 설정으로 복구
      return c;
  }

  int main(void)
  {
      while (1)
      {
          int c = getch();
          printf("%d[%c]", c, c);
      }

      return 0;
  }

  ```
  </details>

  <details>
    <summary><b>#2.</b> getch() 전 kbhit()까지, 터미널 설정을 별도 함수에 따로 작성</summary><br>

  ```c
  // 두 번째는 터미널 설정을 별도 함수에 따로 작성, getch() 전 kbhit()까지
  // kbhit()의 입력 존재 여부 인식은 아래와 같이 select()의 타이머 기능을 사용한 것으로 보인다.
  // getch()는 new_termios에서 getchar()를 사용하는 걸로 간단히! 작성하신 것 같다. 와!

  // https://stackoverflow.com/a/448982 코드

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <sys/select.h>
  #include <termios.h>
  
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

  int getch()
  {
      int r;
      unsigned char c;
      if ((r = read(0, &c, sizeof(c))) < 0) {
          return r;
      } else {
          return c;
      }
  }

  int main(void)
  {
      // "conio" 사용을 위한 터미널 모드 설정
      set_conio_terminal_mode();

      while (1)
      {
          /* do some work */
          int val = kbhit();
          if (val)
          {
              // (void)getch(); /* consume the character */

              int c = getch();
              printf("%d[%c]", c, c);
              fflush(0);

              // CTRL + C
              if (c == ('c' & 037)) break;
          }
      }

      // 기존 터미널 모드로 초기화
      reset_terminal_mode();

      printf("\n");
      fflush(0);
      return 0;
  }

  ```
  </details>

  <details>
    <summary><b>#3.</b> <b>#1</b>, <b>#2</b>를 응용해 non-blocking 형태로 특정 키 입력 확인</summary><br>
      
  ```c
  // 세 번째는 위 두 코드를 응용하여 non-blocking 형태로 특정 키를 인식하도록 구현해 본 것이다.
  // 위느님들처럼 kbhit()는 select()의 타이머 기능을 사용하고,
  // getch()는 수정된 터미널 모드에서 getchar()를 사용하는 기법으로 그대로.

  // int key: 눌렀는지 확인하고 싶은 키를 집어 넣음
  // non-blocking, 즉 입력할 때까지 계속 대기에 있진 않을 것임
  int gett(int key)
  {
      int c;
      struct termios oldattr, newattr;

      tcgetattr(STDIN_FILENO, &oldattr);           // 현재 터미널 설정 읽음
      newattr = oldattr;
      newattr.c_lflag &= ~(ICANON | ECHO);         // CANONICAL과 ECHO 끔
      newattr.c_cc[VMIN] = 1;                      // 최소 입력 문자 수를 1로 설정
      newattr.c_cc[VTIME] = 0;                     // 최소 읽기 대기 시간을 0으로 설정
      tcsetattr(STDIN_FILENO, TCSANOW, &newattr);  // 터미널에 설정 입력

      // KBHIT()
      struct timeval tv = { 0L, 0L };
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(0, &fds);
      if (!select(1, &fds, NULL, NULL, &tv)) {
          tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
          return 0;
      }

      // GETCH()
      c = getchar();                               // 키보드 입력 읽음

      tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);  // 기존 터미널 설정으로 복구

      // 누른 키가 확인하는 키와 일치하는지 여부를 반환
      return c == key;
  }
  ```
  </details>

</details>

<details>
  <summary><b>CODEPOINT TO UNICODE CHARACTERS!</b></summary><br>

  ```c
  // 이건 완벽히 정리하는 건 힘들 듯
  // 깊이 있게 분석하는 건 나중에...

  //
  // https://stackoverflow.com/a/38492214
  // 대박 느님.
  //
  ```

  <br>

  이것의 실질적인 위엄은 [잘 설명된 유니코드 한글 규칙](https://mingpd.github.io/2019/04/09/develop/unicode-hangle/)을 응용해 <br>
  _영문_ 키보드 입력에서의 아스키값 계산을 통한 _한글_ 출력을 가능케 할 수 있다는 것이다! <br>
  <sub>`물론 매우 나중에!`</sub>

  <br>

  ```c
  // __uint8_t 에서:
  // _t의 의미: Cross platform implementation of a standard data type

  #include <stdio.h>

  // Populate utf8 with 0-4 bytes
  // Return length used in utf8[]
  // 0 implies bad codepoint
  unsigned Unicode_CodepointToUTF8(__uint8_t *utf8, __uint32_t codepoint) {
      if (codepoint <= 0x7F) {
          utf8[0] = codepoint;
          return 1;
      }
      if (codepoint <= 0x7FF) {
          utf8[0] = 0xC0 | (codepoint >> 6);
          utf8[1] = 0x80 | (codepoint & 0x3F);
          return 2;
      }
      if (codepoint <= 0xFFFF) {
          // detect surrogates
          if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0;
          utf8[0] = 0xE0 | (codepoint >> 12);
          utf8[1] = 0x80 | ((codepoint >> 6) & 0x3F);
          utf8[2] = 0x80 | (codepoint & 0x3F);
          return 3;
      }
      if (codepoint <= 0x10FFFF) {
          utf8[0] = 0xF0 | (codepoint >> 18);
          utf8[1] = 0x80 | ((codepoint >> 12) & 0x3F);
          utf8[2] = 0x80 | ((codepoint >> 6) & 0x3F);
          utf8[3] = 0x80 | (codepoint & 0x3F);
          return 4;
      }
      return 0;
  }


  // CODEPOINT TO UNICODE CHARACTERS
  // CODEPOINT: 0xAC00('가') 등...
  int main(void)
  {
      // SAMPLE USAGE
      __uint32_t cp = 0xAC01;  // '각'
      __uint8_t utf8[4];
      unsigned len = Unicode_CodepointToUTF8(utf8, cp);
      size_t y = fwrite(utf8, 1, len, stdout);  // '각'이 출력된다!!
  }

  ```
</details>

<!-- <sup><sub>아 참고로 주석은 제가 떠드는 공간인 거 아시죠??</sub></sup> -->
