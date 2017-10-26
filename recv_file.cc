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

#define PORTNUMBER      5193            /* default protocol port number */
#define BSIZE           40              /* size of data buffer          */
#define TIMEOUT         500

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

  int n = poll(pollarr,1,TIMEOUT); // 1 = array size, 100 = timeout value in ms
  // Received, read and go ahead
  if(n > 0){
    return false;
  }
  // Timeout!
  else if(n == 0) {
    return true;
  }
  else {
    cout << "Other error\n"; exit(1);
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

  /* Set up address for local socket */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;           /* set family to Internet     */
  sad.sin_addr.s_addr = INADDR_ANY;   /* set the local IP address   */
  sad.sin_port = htons((u_short)PORTNUMBER);/* convert to network byte order */

  /* Initialize package drop rate. */
  ninit(0, 0);

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
  if((hptr = gethostbyaddr((char*)&sad.sin_addr,
			   sizeof(struct sockaddr_in),AF_INET))==0){
    printf("%s port %d\n",
	   inet_ntoa(sad.sin_addr),ntohs(sad.sin_port));
  }
  else {
    printf("%s/%s port %d\n",
	   hptr->h_name,inet_ntoa(sad.sin_addr),ntohs(sad.sin_port));
  }

  /* Main server loop - receive and handle requests */
  string file_name = "response";
  FILE* f_recv = fopen(file_name.c_str(), "wb");
  char my_seq_num = '0';
  int total = 0;
  
  while (true) {
    /* Inner loop.  Read and echo data received from client. */
    cout << endl;
    cout << "<----------->" << endl;

    bool timed_out = time_out(sd);
    //    bool timed_out = false;
    
    /* While there is a timeout, keep asking for payload. 
       This happens if the payload gets lost through 'sender->receiver'. */
    while (timed_out) {
      cout << "Timeout, resending!" << endl;
      char resp[2];
      memset(resp, 0, 2);
      resp[0] = '2'; // 2 = ACK
      cout << "SENDING ACK 1\n";
      resp[1] = my_seq_num;
      int sent = nsendto(sd, resp, 2, 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      timed_out = time_out(sd);
    }
    mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad,
		    &fromlen);
    cout << "received: " << buf << endl;
    cout << "size: " << mlen << endl;
    /* Once there's something to receive, we receive it. */
    // mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad,
    //     &fromlen);
    if(mlen<0){
      perror("recvfrom");
      exit(1);
    }
    /* CRC code in buf[mlen-2] and buf[mlen-1]! */
    char msg_type = buf[0];
    char sender_seq_num = buf[1];

    /* File transfer is done! Break out, handle termination. */
    if (msg_type == '4') {
      break;
    }
    /* This basically means sender didn't get my last ACK. 
       So I resend it and don't write payload to file (would be a 
       a duplicate). */
    if (my_seq_num != sender_seq_num) {
      cout << "Unsynced seq numbers (lost ACK)! Resending.\n";
      /* You should modularize this since it's repeated code from below.*/
      char resp[2];
      memset(resp, 0, 2);
      resp[0] = '2'; // 2 = ACK
      cout << "SENDING ACK 2.\n";
      resp[1] = sender_seq_num;
      int sent = nsendto(sd, resp, 2, 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      continue;
    }

    /* Extracting payload data. */
    char payload_data[BSIZE];
    memset(payload_data, 0, BSIZE);
    for (int i = 2, j = 0; i < mlen-2; i++, j++) {
      payload_data[j] = buf[i]; 
    }
    cout << "Payload data: " << payload_data << endl;
    cout << endl;
    memset(buf, 0, sizeof(buf));

    /* Write payload data to file! */
    int w = fwrite(payload_data, 1, mlen-4, f_recv);
    total += w;

    /*  Display a message showing the client's address */
    printf("%d bytes received from ", mlen);
    if((hptr = gethostbyaddr((char*)&cad.sin_addr,
			     sizeof(struct sockaddr_in),AF_INET))==0){
      printf("%s port %d\n",
	     inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
    }
    else {
      printf("%s/%s port %d\n",
	     hptr->h_name,inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
    }
    cout << "Receiver: Wrote " << w << " to file.\n";
    /*  Send ACK to client verifying that we got the payload and
        wrote it to file. */
    char resp[BSIZE];
    memset(resp, 0, BSIZE);
    resp[0] = '2'; // 2 = ACK
    cout << "SENDING ACK 3\n";
    resp[1] = sender_seq_num; // 1 or 0
    int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
    if (sent < 0) cout << "ERROR\n";
    my_seq_num = advance_seq_num(my_seq_num);
  }

  cout << "FIN received" << endl;
  char resp[1];
  memset(resp, 0, 1);
  resp[0] = '5'; // 5 = FINACK
  cout << endl;
  cout << "Sending FINACK\n";
  int sent = sendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
  if (sent < 0) cout << "ERROR\n";
  /*
    I don't understand where an incorrect sequence number is being sent. This is preventing
    the sender from acknowledging that I am sending back a FINACK! Because whenever I handle response
    on sender side, it thinks I've sent an ACK frame with an incorrect sequence number.
  */
  //      exit(1);

  cout << "wrote a total of: " << total << endl;
  // Handle termination! 
  fclose(f_recv);
}
