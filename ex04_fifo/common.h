#ifndef COMMON_H
#define COMMON_H

#define RTF_COMMAND_NUM 0
#define RTF_COMMAND_DEV "/dev/rtf0"

#define RTF_STATUS_NUM  1
#define RTF_STATUS_DEV "/dev/rtf1"

#define RTF_SIZE 1024		/* how big the FIFOs are */

enum etype {SOUND_ON = 1, SOUND_OFF, SOUND_FREQ};

typedef struct {
  enum etype command;
  int command_num;		/* identifies command */
  int freq;			/* for SOUND_FREQ */
} COMMAND_STRUCT;

typedef struct {
  int command_num_echo;		/* acknowledge */
  int freq;			/* actual frequency */
  int heartbeat;		/* incremented each cycle */
} STATUS_STRUCT;

#endif /* COMMON_H */
