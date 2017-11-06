#define PAYLOAD_SIZE 36 // 40 - type(1) - seq num(1) - crc (2)

#include <stdio.h>

enum frame_type {
  DATA, ACK, NACK, FIN, FINACK
};

struct frame {
  frame_type frame;
  int seq_num;
  char data[PAYLOAD_SIZE];
  char crc[2];
};
