#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

using namespace std;

uint16_t getCRC(char * message, int len){
  uint16_t crc = 0x0000;
  uint16_t G = 0x8005;

  for (int i = 0; i < len; i++){
    uint8_t byte = message[i];
    for (int j = 7; j >= 0; j--){
      if ((crc & 0x8000) != 0){ // If MSB is set in CRC code.
	crc = crc << 1;
	if ((byte & (1 << j)) != 0){
	  crc |= 0x0001; // Set LSB to 1
	}
	else {
	  crc |= 0x0000; // Set LSB to 0
	}
	crc ^=  G;
      }
      else {
	crc = crc << 1;
	if ((byte & (1<<j)) != 0){
	  crc = crc |= 0x0001; // Set LSB to 1
	}
	else {
	  crc = crc |= 0x0000; // Set LSB to 0
	}
      }
    }
  }
  printf("\n0x%x\n", crc);
  return crc;
}

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

// int main() {
//   char* buf = "abcd";
//   char* buf2 = "abcd00";
//   getCRC(buf2, 6);
//   uint16_t crc2 = getCRC2(buf, 4);
//   int left = crc2 & 0xFF00;
//   int right = crc2 & 0xFF;
//   printf("\n0x%x\n", left + right);
//   printf("\n0x%x\n", right);
//       //  crc3(buf, 6);
//   return 0;
// }


