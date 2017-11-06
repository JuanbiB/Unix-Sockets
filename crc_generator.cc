#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

using namespace std;
int getCRC2(char * message, int len){
    unsigned short crc = 0x0000;
    unsigned short generator = 0x8005;
    
    for (int i = 0; i < len; i++){
      crc ^= ((uint16_t)message[i] << 8);
      
      for (int j = 0; j < 8; j++){
	if ((crc & 0x8000) != 0) {
	  crc = ((uint16_t)crc << 1) ^ generator;
	}
	else {
	  crc <<= 1;
	}
      }
    }
    return crc;
}


