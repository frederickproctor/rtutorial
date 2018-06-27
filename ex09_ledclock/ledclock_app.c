/*
  ledclock_app.c

  Adapted from software written by Stuart Hughes, formerly of Lineo, Inc.,
  shughes@lineo.com, seh@zee2.com. Released under the Gnu General Public
  License.
 */

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

unsigned char vfont[9 * 256];
unsigned char hfont[9 * 256];
unsigned char obuf[1024];

unsigned char key_in();
void tofifo(unsigned char c, int n);
static int ff;

static int done = 0;
static void quit(int sig)
{
  done = 1;

  return;
}

int main()
{
  FILE *fn;
  unsigned char c;
  int i, j, n = 0;

  if ((fn = fopen("default8x9", "r")) == 0) {
    fprintf(stderr, "fopen(default8x9) : %s\n", strerror(errno));
  }
  fread(vfont, 9 * 256, 1, fn);
  fclose(fn);

  for (i = 0; i < 256; i++) {
    for (c = 0, j = 0; j < 8; j++) {
      c = ((vfont[(i * 9) + 7] & 1 << j) ? 1 : 0) << 0;
      c |= ((vfont[(i * 9) + 6] & 1 << j) ? 1 : 0) << 1;
      c |= ((vfont[(i * 9) + 5] & 1 << j) ? 1 : 0) << 2;
      c |= ((vfont[(i * 9) + 4] & 1 << j) ? 1 : 0) << 3;
      c |= ((vfont[(i * 9) + 3] & 1 << j) ? 1 : 0) << 4;
      c |= ((vfont[(i * 9) + 2] & 1 << j) ? 1 : 0) << 5;
      c |= ((vfont[(i * 9) + 1] & 1 << j) ? 1 : 0) << 6;
      c |= ((vfont[(i * 9) + 0] & 1 << j) ? 1 : 0) << 7;
      hfont[(i * 9) + (7 - j)] = c;
    }
  }

  if ((ff = open("/dev/rtf0", O_WRONLY)) < 0) {
    fprintf(stderr, "open(/dev/rtf0) : %s\n", strerror(errno));
  }

  done = 0;
  signal(SIGINT, quit);

  while (! done) {
    usleep(100000);
    if ((c = key_in()) == 0) {
      continue;
    }
    if (n * 8 >= sizeof(obuf) || c == '\n' || c == '\r') {
      // clear the line
      memset(obuf, 0, sizeof(obuf));
      write(ff, obuf, sizeof(obuf));
      n = 0;
      continue;
    }
    n++;
    if (c == 127) {
      n--;
      if (n == 0) {
	continue;
      }
      tofifo(' ', n);
      n--;
      continue;
    }
    tofifo(c, n);
  }

  return 0;
}

void tofifo(unsigned char c, int n)
{
  //fprintf(stderr, "n=%d, %d\n", n, c);
  memcpy(&obuf[(n - 1) * 8], &hfont[9 * c], 8);
  write(ff, obuf, n * 8);
}

unsigned char key_in()
{
  static int init = 0;
  static struct termios tty, otty;
  unsigned char c;

  if (init == 0) {
    tcgetattr(0, &tty);
    otty = tty;
    tty.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &tty);
    fcntl(0, F_SETFL, O_NONBLOCK);
    init = 1;
    return 0;
  }

  if ((read(0, &c, 1) == 1)) {
    return c;
  }
  return 0;
}
