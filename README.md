# TCP multiple clients

`18WP 2021`

<br>

Server source   | Client source
--------------- | -------------
`mulfd.c`       | `mulcli.c`

Cmd code          | 1000                | 2000                | 3000
----------------- | ------------------- | ------------------- | ---------------------
**Signification** | Message from server | Set client nickname | Message communication

##

**`would-be helpful notes to self`**

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

<br>

## Nickname requisites

- Must not start with a `SPACE` character
- Must be shorter than `NAME_SIZE (30)` characters
- All names must be unique

<br>

## "Emojis"

`temporarily? doubled the BUF_SIZE because of it`

Command | `:myh:` |
------- | ------- |
**Appearance** | ⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢠⣴⣾⣿⣶⣶⣆⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀⢀<br>⢀⢀⢀⣀⢀⣤⢀⢀⡀⢀⣿⣿⣿⣿⣷⣿⣿⡇⢀⢀⢀⢀⣤⣀⢀⢀⢀⢀⢀<br>⢀⢀ ⣶⢻⣧⣿⣿⠇ ⢸⣿⣿⣿⣷⣿⣿⣿⣷⢀⢀⢀⣾⡟⣿⡷⢀⢀⢀⢀<br>⢀⢀⠈⠳⣿⣾⣿⣿⢀⠈⢿⣿⣿⣷⣿⣿⣿⣿⢀⢀⢀⣿⣿⣿⠇⢀⢀⢀⢀<br>⢀⢀⢀⢀⢿⣿⣿⣿⣤⡶⠺⣿⣿⣿⣷⣿⣿⣿⢄⣤⣼⣿⣿⡏⢀⢀⢀⢀⢀<br>⢀⢀⢀⢀⣼⣿⣿⣿⠟⢀⢀⠹⣿⣿⣿⣷⣿⣿⣎⠙⢿⣿⣿⣷⣤⣀⡀⢀⢀<br>⢀⢀⢀ ⢸⣿⣿⣿⡿⢀⢀⣤⣿⣿⣿⣷⣿⣿⣿⣄⠈⢿⣿⣿⣷⣿⣿⣷⡀⢀<br>⢀⢀⢀⣿⣿⣿⣿⣷⣀⣀⣠⣿⣿⣿⣿⣷⣿⣷⣿⣿⣷⣾⣿⣿⣿⣷⣿⣿⣿⣆<br>⣿⣿⠛⠋⠉⠉⢻⣿⣿⣿⣿⡇⡀⠘⣿⣿⣿⣷⣿⣿⣿⠛⠻⢿⣿⣿⣿⣿⣷⣦<br>⣿⣿⣧⡀⠿⠇⣰⣿⡟⠉⠉⢻⡆⠈⠟⠛⣿⣿⣿⣯⡉⢁⣀⣈⣉⣽⣿⣿⣿⣷<br>⡿⠛⠛⠒⠚⠛⠉⢻⡇⠘⠃⢸⡇⢀⣤⣾⠋⢉⠻⠏⢹⠁⢤⡀⢉⡟⠉⡙⠏⣹<br>⣿⣦⣶⣶⢀⣿⣿⣿⣷⣿⣿⣿⡇⢀⣀⣹⣶⣿⣷⠾⠿⠶⡀⠰⠾⢷⣾⣷⣶⣿<br>⣿⣿⣿⣿⣇⣿⣿⣿⣷⣿⣿⣿⣇⣰⣿⣿⣷⣿⣿⣷⣤⣴⣶⣶⣦⣼⣿⣿⣿⣷ |
