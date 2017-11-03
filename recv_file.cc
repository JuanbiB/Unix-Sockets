#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "nsendto.c"
#include "crc_generator.cc"

#define BSIZE           40              /* size of data buffer */

using namespace std;

char advance_seq_num (char seq_num) {
  if (seq_num == '0')
    return '1';
  return '0';
}

int main(int argc, char**argv)
{
  struct  sockaddr_in sad; /* structure to hold server's address  */
  struct  sockaddr_in cad; /* structure to hold client's address  */
  socklen_t fromlen = sizeof(cad);
  int     sd;              /* socket descriptor                   */
  int     mlen;            /* byte count for the current request  */
  char    buf[BSIZE];      /* buffer for string the server sends  */
  struct hostent *hptr;

  char* host_name;
  int port_number;
  char* file_name;
  double drop_p;
  double byte_err_p;

  if (argc > 4) {
    port_number = atoi(argv[1]);
    file_name = argv[2];
    char* ptr;
    drop_p = strtod(argv[3], &ptr);
    byte_err_p = strtod(argv[4], &ptr);
  }
  else {
    cout << "Not enough arguments.\n";
    exit(1);
  }

  /* Initialize package drop rate. */
  ninit(drop_p, byte_err_p);

  /* Set up address for local socket */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;           /* set family to Internet     */
  sad.sin_addr.s_addr = INADDR_ANY;   /* set the local IP address   */
  sad.sin_port = htons((u_short)port_number);/* convert to network byte order */

  /* Create a socket */
  sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sd < 0) {
    perror("socket creation");
    exit(1);
  }

  /* Bind a local address to the socket */
  if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    perror("bind");
    exit(1);
  }
  cout << "bound to socket!" << endl;
  if((hptr = gethostbyaddr((char*)&sad.sin_addr, sizeof(struct sockaddr_in),AF_INET)) == 0) {
    printf("%s port %d\n", inet_ntoa(sad.sin_addr),ntohs(sad.sin_port));
  }
  else {
    printf("%s/%s port %d\n", hptr->h_name,inet_ntoa(sad.sin_addr),ntohs(sad.sin_port));
  }

  /* Main server loop - receive and handle requests */
  FILE* f_recv = fopen(file_name, "wb");
  char my_seq_num = '0';
  int total = 0;
  
  while (true) {
    /* Inner loop.  Read and echo data received from client. */

    mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad, &fromlen);
    if (mlen < 0) {
      perror("recvfrom");
      exit(1);
    }

    char msg_type = buf[0];
    int sender_seq_num = buf[1];
    cout << "Received seq num: " << (sender_seq_num % 7) << endl;

    /* File transfer is done! Break out, handle termination. */
    if (msg_type == '4') {
      break;
    }

    /* CRC code in buf[mlen-2] and buf[mlen-1]! */
    uint8_t crc1 = buf[mlen-2];
    uint8_t crc2 = buf[mlen-1];

    uint16_t crc_generated = getCRC2(buf, mlen);
    
    /* This means we got a payload with errors in it, so we want
       to ask for it again, so we just continue so they timeout
       and resend payload.. */
    if (crc_generated != 0) {
      cout << "We have a bit error!\n";
      continue;
    }

    /* Extracting payload data. */
    char payload_data[BSIZE];
    memset(payload_data, 0, BSIZE);
    for (int i = 2, j = 0; i < mlen-2; i++, j++) {
      payload_data[j] = buf[i]; 
    }


    /* Write payload data to file! */
    int w = fwrite(payload_data, 1, mlen-4, f_recv);
    total += w;

    /*  Send ACK to client verifying that we got the payload and
        write it to file. */
    char resp[BSIZE];
    memset(resp, 0, BSIZE);
    resp[0] = '2'; // 2 = ACK
    resp[1] = sender_seq_num;// + 1 
    int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
    if (sent < 0) cout << "ERROR\n";
    memset(buf, 0, sizeof(buf));
  }

  // Handle termination!
  fclose(f_recv);
  char resp[1];
  memset(resp, 0, 1);
  resp[0] = '5'; // 5 = FINACK
  int sent = sendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
  if (sent < 0) cout << "ERROR\n";
  // wait for ACK from receiver, since receiver has acknowledged my FINACK.
  mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad, &fromlen);
  if (mlen < 0) {
    perror("recvfrom");
    exit(1);
  }
  char msg_type = buf[0];
  char sender_seq_num = buf[1];
  if (msg_type == '2') {
    cout << "wrote a total of: " << total << " bits." << endl;
    cout << "...program terminated." << endl;
    exit(1);
  }
  close(sd);
  exit(0);
}
