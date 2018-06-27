/*
  isr_app.c

  Just prints out occurrences of interrupts by reading a FIFO.
*/

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>
#include <signal.h>		/* signal(), SIGINT */
#include <unistd.h>		/* open(), close() */
#include <fcntl.h>		/* O_RDONLY */
#include "isr_common.h"		/* FIFO_NAME */

/*
  We can attach this null function as the signal handler for SIGINT,
  which is raised when you hit Ctrl-C or sent by 'kill -INT <pid>'
  to this process' process id (pid) in the 'run' script. This overrides
  the default action (kill the process immediately), allowing the
  early return from 'read' to be caught and handled gracefully.
 */
static void quit(int sig)
{
  return;
}

int main()
{
  int fd;
  int interrupts;

  /* open RT FIFO */
  if ((fd = open(FIFO_NAME, O_RDONLY)) < 0) {
    fprintf(stderr, "error opening %s\n", FIFO_NAME);
    return 1;
  }

  /*
    Trap SIGINT, which will be sent to us eventually as a signal to
    quit, thus breaking the 'read' operation below and allowing us
    to terminate gracefully.
   */
  signal(SIGINT, quit);

  while (1) {
    /* read cumulative interrupts from the fifo */
    if (sizeof(interrupts) == read(fd, &interrupts, sizeof(interrupts))) {
      printf("cumulative interrupts: %d\n", interrupts);
    } else {
      /* the read was interrupted, presumably by SIGINT, so we quit */
      break;
    }
  }

  close(fd);		  /* kindergarten: if you open it, close it */

  return 0;
}
