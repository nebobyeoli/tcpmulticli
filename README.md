# TCP multiple clients

#### `18WP 2021`

## Sources

Server source   | Client source
--------------- | -------------
[`mulfd.c (main branch)`](https://github.com/nebobyeoli/tcpmulticli/blob/main/mulfd.c) | [`mulcli.c (main branch)`](https://github.com/nebobyeoli/tcpmulticli/blob/main/mulcli.c)

## Branches

Branch name | Pull request | Description
----------- | ------------ | -----------
**clistat** | [Manage Client Status](https://github.com/nebobyeoli/tcpmulticli/pull/2) | Server/Client 필요 기능 조건
**emojis**  | [Emoji support](https://github.com/nebobyeoli/tcpmulticli/pull/1)        | 텍스트 이미지 이모티콘 및 긴 `mms` 송수신에 관하여

## Cmd code significations

Cmd code | Meaning
-------- | ---------------------
**1000** | Message from server
**2000** | Set client nickname
**3000** | Message communication

## Message data

Order | Name          | Size variable  | Size
----- | ------------- | -------------- | ----------
1     | **`cmdcode`** | `CMDCODE_SIZE` | `4`
2     | **`sender`**  | `NAME_SIZE`    | `30`
3     | **`msg`**     | `BUF_SIZE`     | `1024 * n`

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

### 소스 중 특정 일부 참고용

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
  <summary><b>Mimicking str.replace functioning behavior in C</b></summary><br>

  ```c
  // IMPLEMENET SUBSTRING REPLACEMENT

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
