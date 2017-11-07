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
#define TIMEOUT         100

using namespace std;

char advance_seq_num (char seq_num) {
  if (seq_num == '0')
    return '1';
  return '0';
}

bool time_out(int sd) {
  // Set up a pollfd structure
  struct pollfd pollstr;
  pollstr.fd = sd; // the file descriptor for the socket you are using
  pollstr.events = POLLIN; // the events on the descriptor that you want to poll for
  struct pollfd pollarr[1];
  pollarr[0] = pollstr;

  int n = poll(pollarr, 1, TIMEOUT); // 1 = array size, 100 = timeout value in ms
  // Received, read and go ahead
  if(n > 0) {
    return false;
  }
  // Timeout!
  else if(n == 0) {
    return true;
  }
  else {
    cout << "Other error\n";
    exit(1);
  }
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
  int my_seq_num = 0;
  int total = 0;

  int failed_datagrams = 0;
  int acks_sent = 0;
  int ack_resends = 0;
  int time_outs = 0;
    
  while (true) {
    /* Continue received data from client. */
    mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad, &fromlen);
    if (mlen < 0) {
      perror("recvfrom");
      exit(1);
    }

    char msg_type = buf[0];
    int sender_seq_num = (buf[1] - '0');
    cout << "--------------------" << endl;
    cout << "Received seq num: " << sender_seq_num << endl;
    cout << "expected  seq num " << my_seq_num << endl;

    /* File transfer is done! Break out, handle termination. */
    if (msg_type == '4') {
      cout << "FIN RECEIVED\n";
      break;
    }
    
    /* CRC code in buf[mlen-2] and buf[mlen-1]! */
    uint8_t crc1 = buf[mlen-2];
    uint8_t crc2 = buf[mlen-1];

    uint16_t crc_generated = getCRC2(buf, mlen);

    /* If we get a payload with data error or a wrong sequence number, 
       we ignore it... The sender will time out and resend window. */
    if ((my_seq_num != sender_seq_num) || (crc_generated != 0)) {
      if (crc_generated != 0) failed_datagrams++;
      
      cout << "Rejected packet. \n";
      if ((my_seq_num != sender_seq_num) && mlen < 40){
	/* If this is the last payload they're going to send us, help the 
	   sender terminate by re-sending my ack. */
	char resp[BSIZE];
	memset(resp, 0, BSIZE);
	resp[0] = '2';
	resp[1] = my_seq_num + '0';
	uint16_t crc = getCRC2(resp, strlen(resp));
	uint8_t left = crc  >> 8;
	uint8_t right = crc & 0xFF;
	resp[2] = left;
	resp[3] = right;
	ack_resends++;
	int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
      }
      /* Otherwise we want them to just time out and resend window. */
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

    /* Update my expected sequence number. */
    cout << "Increasing..\n";
    my_seq_num = (my_seq_num+1) % 8;
    cout << "Requesting: " << my_seq_num << endl;
    resp[1] = my_seq_num + '0';
    resp[2] = w + '0';
    uint16_t crc = getCRC2(resp, strlen(resp));
    
    uint8_t left = crc  >> 8;
    uint8_t right = crc & 0xFF;
    resp[3] = left;
    resp[4] = right;
    
    int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
    if (sent < 0) cout << "ERROR\n";
    memset(buf, 0, sizeof(buf));
  }

  // Handle termination!
  bool timed_out = time_out(sd);
  while (true) {
    if (!timed_out) {
      fclose(f_recv);
      /* At this point we have received the FIN message. Now send a FINACK and
         wait for an ACK packet. */
      char resp[1];
      memset(resp, 0, 1);
      resp[0] = '5'; // 5 = FINACK
      acks_sent++;
      int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      break;
    }
    else { // we have timed out.
      char resp[1];
      memset(resp, 0, 1);
      resp[0] = '2'; // 2 = ACK
      acks_sent++;
      int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      cout << "ACK RESENT\n";
      timed_out = time_out(sd);
    }
  }
  // wait for ACK from receiver, since receiver has acknowledged my FINACK.
  timed_out = time_out(sd);
  while (true) {
    if (!timed_out) {
      mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad, &fromlen);
      if (mlen < 0) {
        perror("recvfrom");
        exit(1);
      }
      break;
    }
    else {
      char resp[1];
      memset(resp, 0, 1);
      resp[0] = '5'; // 5 = FINACK
      acks_sent++;
      int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      cout << "FINACK RESENT\n" ;
      timed_out = time_out(sd);
    }
  }
  char msg_type = buf[0];
  cout << "msg_type: " << msg_type << endl;
  char sender_seq_num = buf[1];
  if (msg_type == '2') {
    cout << "...program terminated." << endl;
  }

  cout << "\n<----- Summary ----->\n";
  cout << "Wrote to file: " << total << " bits." << endl;
  cout << "Datagrams that failed CRC: " << failed_datagrams << endl;
  cout << "ACKs sent: " << acks_sent << endl;
  cout << "ACK resends: " << ack_resends << endl;
  cout << "Timeouts: " << time_outs << endl;
  cout << endl;
  
  close(sd);
  exit(0);
}
