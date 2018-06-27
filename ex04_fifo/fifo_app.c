/*
  fifo_app.c

  Shows how to set up a normal user-level Linux process to communicate
  with an RT task using FIFOs. FIFOs are queues of data, first-in-first-out,
  that are set up as Linux character devices much like a serial port.
  There are 64 FIFOs, named /dev/rtf0 through /dev/rtf63. These can be
  accessed as usual for character devices, using the Unix open(), read(),
  write() and close() functions.

  This program opens one FIFO to the RT task for commands, and another
  from the RT task for status back. There is handshaking in the RT task
  such that status is written back for each command. This simplifies
  programming here. 

  In general, a user task may have numerous FIFOs open between itself
  and an RT task. The Unix function select() can be used to read from 
  the first available FIFO. We don't show that here.
*/

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>		/* open() */
#include <fcntl.h>		/* O_RDONLY */
#include "common.h"		/* declarations for our command and status */

int main()
{
  enum {BUFFERLEN = 80};
  char buffer[BUFFERLEN];
  COMMAND_STRUCT command;
  STATUS_STRUCT status;
  int command_fd;
  int status_fd;
  int freq;
  int retval;

  /*
    Open the command and status FIFOs.
   */

  if ((command_fd = open(RTF_COMMAND_DEV, O_WRONLY)) < 0) {
    fprintf(stderr, "error opening %s\n", RTF_COMMAND_DEV);
    return 1;
  }

  if ((status_fd = open(RTF_STATUS_DEV, O_RDONLY)) < 0) {
    fprintf(stderr, "error opening %s\n", RTF_STATUS_DEV);
    return 1;
  }

  /*
    Now we'll enter a loop, reading input from the terminal, parsing
    it and composing a command written to the RT task FIFO. After this
    we wait for a response, print it and continue.

    We can handle "on", "off", and "freq <Hz>", where "on" turns the
    speaker on at the last set frequency, "off" turns it off, and
    <Hz> is the frequency we want to set. "quit" quits.
   */

  command.command_num = 0;
  retval = 0;

  printf("Enter 'on', 'off' or a frequency. 'q' quits.\n");
  printf("The command serial number, frequency and hearbeat are printed.\n");

  while (! feof(stdin)) {
    printf("> ");
    fflush(stdout);
    if (NULL == fgets(buffer, BUFFERLEN, stdin)) break;

    /* parse the buffer and compose a command */
    if (! strncmp(buffer, "on", 2)) {
      command.command = SOUND_ON;
    } else if (! strncmp(buffer, "off", 3)) {
      command.command = SOUND_OFF;
    } else if (1 == sscanf(buffer, "%d", &freq)) {
      command.command = SOUND_FREQ;
      command.freq = freq;
    } else if (! strncmp(buffer, "q", 1)) {
      break;
    } else {
      /* unknown command */
      printf("?\n");
      continue;
    }

    /* if we dropped through to here, it's time to send the command */
    command.command_num++;
    if (sizeof(command) != write(command_fd, &command, sizeof(command))) {
      fprintf(stderr, "can't write command\n");
      retval = 1;
      break;
    }

    /* now check for the reply */
    if (sizeof(status) == read(status_fd, &status, sizeof(status))) {
      printf("status: %d %d %d\n",
	     status.command_num_echo, status.freq, status.heartbeat);
    }
  }

  close(status_fd);
  close(command_fd);

  return retval;
}
