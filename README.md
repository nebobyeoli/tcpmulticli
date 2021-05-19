# TCP multiple clients

`18WP 2021`

<br>

Server source   | Client source
--------------- | -------------
`mulfd.c`       | `mulcli.c`

Cmd code          | 1000                | 2000
----------------- | ------------------- | ---------------------
**Signification** | Set client nickname | Message communication

<br><br>

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
