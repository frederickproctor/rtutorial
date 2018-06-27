/*
  rcservo_app.c
*/

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>
#include <unistd.h>		/* open() */
#include <fcntl.h>		/* O_RDONLY */
#include "common.h"

int main()
{
  enum {BUFFERLEN = 80};
  char buffer[BUFFERLEN];
  COMMAND_STRUCT command;
  int fd;
  int retval;

  /* open RT FIFO 0 */
  if ((fd = open("/dev/rtf0", O_WRONLY)) < 0) {
    fprintf(stderr, "error opening /dev/rtf0\n");
    return 1;
  }

  retval = 0;

  /* loop on user input of two numbers, which motor 0..RC_NUM and
     target position, -1000..1000 */

  while (! feof(stdin)) {
    printf("enter [0 1] [-1000 to 1000], ^D to quit: ");
    fflush(stdout);
    if (NULL == fgets(buffer, BUFFERLEN, stdin)) break;

    /* we got a line, should be of form <0..RC_NUM> <-1000..1000> */
    if (2 != sscanf(buffer, "%d %d", &command.which, &command.position)) {
      /* bad format */
      printf("?\n");
      continue;
    }

    /* else we got a good command, so write it */
    if (sizeof(command) != write(fd, &command, sizeof(command))) {
      fprintf(stderr, "can't write command\n");
      retval = 1;
      break;
    }
  }

  close(fd);

  return retval;
}
