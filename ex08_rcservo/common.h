#ifndef COMMON_H
#define COMMON_H

#define RC_NUM 3

typedef struct {
  int which;			/* which motor to set, 0..RC_NUM */
  int position;			/* what position to set, -1000..1000 */
} COMMAND_STRUCT;

#endif /* COMMON_H */

