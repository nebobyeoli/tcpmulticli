# TCP multiple clients

#### `18WP 2021`

```c
// 지정된 멀티캐스팅 주소
char mulcast_addr[] = "239.0.100.1";
```

<sub>문서 작업은 고단한 일입니다!</sub>

<sup><sup>제가 확실히 알지 못하는 코드 주석은 <sub>거의</sub> 작성하지 않습니다!</sup></sup>

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
./mulfd  <PORT>
./mulcli <IP> <PORT>
```

## Branches

Branch name | Pull request | Description
----------- | ------------ | -----------
**clistat** | [Manage Client Status](https://github.com/nebobyeoli/tcpmulticli/pull/2) | Server/Client 필요 기능 조건
**emojis**  | [Emoji support](https://github.com/nebobyeoli/tcpmulticli/pull/1) | 텍스트 이미지 이모티콘 및 긴 `mms` 송수신에 관하여
**keyinput**  | [Keyboard input by character](https://github.com/nebobyeoli/tcpmulticli/pull/4) | `termios`를 이용한 사용자 정의 `kbhit()` 및 `getch()` 활성화 <br> 글자를 하나씩 입력받아 직접 할당하는 방식으로의 입력 구현 <br> 즉 `엔터` 없이도, 입력 `중의` 입력 버퍼를 직접 관리할 수 있도록 하는 작업

## Cmd code significations

Cmd code | Meaning
-------- | ---------------------
**1000** | Message from server
**2000** | Set client nickname
**3000** | Message communication

## Message format

Name              | `cmdcode`      | `sender`    | `msg`
----------------- | -------------- | ----------- | ----------
**Size constant** | `CMDCODE_SIZE` | `NAME_SIZE` | `BUF_SIZE`
**Size**          | `4`            | `30`        | `1024 * n`

Message is concatenated via `sprintf()`.

Example:
```c
// CREATE MESSAGE FOR write()
// + 1 AND + 2 BELOW INDICATES LEAVING OUT [NULL] CHARACTERS
// AS SEPARATORS TO DISTINGUISH FORMAT PARAMETERS ON read()

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
```

## Nickname requisites

- Must not start with a `SPACE` character
- Must be shorter than `NAME_SIZE (30)` characters
- All names must be unique

## Emoji support

`temporarily. quadrupled the BUF_SIZE because of it`

Command   | Description | Appearance
--------- | ----------- | ----------
**:myh:** | 무 야 호 | ⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀<br>⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀<br>⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀<br>⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀<br>⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀<br>⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀<br>⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀<br>⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆<br>⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦<br>⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷<br>⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹<br>⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿<br>⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷ |

## Notes to self.

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

##

### `알아낸 거 목록`

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

  // In the linux terminal you may use terminal commands to move your cursor, such as
  printf("\033[8;5Hhello");   // Move to (8, 5) and output hello

  // other similar commands:
  printf("\033[XA");  // Move up X lines;
  printf("\033[XB");  // Move down X lines;
  printf("\033[XC");  // Move right X columns;
  printf("\033[XD");  // Move left X columns;
  printf("\033[2J");  // Clear screen

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

<sup><sub>아 참고로 주석은 제가 떠드는 공간인 거 아시죠??</sub></sup>
