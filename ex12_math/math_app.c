/*
  math_app.c

  Runs a demonstrative floating point calculation that adds the numbers 
  from 1 to 10 million and prints the result. Unbeknownst to this
  application (and all other Linux processes), the real-time task alternately
  enabling and disabling floating point unit (FPU) save/restore, so that
  when this process is interrupted during its calculation, its intermediate
  results are destroyed, resulting in bad numbers or even "not-a-number"
  (nan) values. The output will look something like this:

  ...
  50000005000000.000000
  50000005000000.000000
  ...
  nan
  nan
  nan
  ...
  50000005000000.000000
  50000005000000.000000
  ...

  Moral: if your RT Linux task uses floating point, make sure to enable
  FPU save/restore in that task. It's off by default to save time.
*/

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>
#include <signal.h>

static int done = 0;
static void quit(int sig)
{
  done = 1;
}

int main(void)
{
  double a, s;

  signal(SIGINT, quit);
  done = 0;

  while (! done) {
    for (a = s = 0.0; a < 1.0e7; a += 1.0, s += a) ;
    printf("%f\n", s);
  }

  return 0;
}
