#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "nsendto.c"

#define PORTNUMBER      5193            /* default protocol port number */
#define BSIZE           40            /* size of data buffer          */

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

  /* Set up address for local socket */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;           /* set family to Internet     */
  sad.sin_addr.s_addr = INADDR_ANY;   /* set the local IP address   */
  sad.sin_port = htons((u_short)PORTNUMBER);/* convert to network byte order */

  /* Initialize package drop rate. */
  ninit(0.7, 0);

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
  string file_name = "response.txt";
  FILE* f_recv = fopen(file_name.c_str(), "w");
  char my_seq_num = '0';
  
  while (true) {
    memset(buf, 0, sizeof(buf));
    /* Inner loop.  Read and echo data received from client. */
    mlen = recvfrom(sd, buf, BSIZE, 0, (struct sockaddr *)&cad,
		    &fromlen);
    
    /* CRC code in buf[mlen-2] and buf[mlen-1]! */
    char msg_type = buf[0];
    char sender_seq_num = buf[1];

    /* File transfer is done! Break out, handle termination. */
    if (msg_type == '4') break; // 4 = FIN

    cout << "Sender seq num: " << sender_seq_num << endl;
    cout << "My seq num: " << my_seq_num << endl;

    /* This basically means sender didn't get my last ACK. 
       So I resend it and don't write payload to file (would be a 
       a duplicate). */
    if (my_seq_num != sender_seq_num) {
      cout << "Unsynced seq numbers! They lost my ACK\n";
      /* You should modularize this since it's repeated code from below.*/
      char resp[BSIZE];
      memset(resp, 0, BSIZE);
      resp[0] = '2'; // 2 = ACK
      resp[1] = sender_seq_num; // 1 or 0
      int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
      if (sent < 0) cout << "ERROR\n";
      continue;
    }

    /* Extracting payload data. */
    char payload_data[BSIZE];
    memset(payload_data, 0, BSIZE);
    for (int i = 2, j = 0; i < mlen-2; i++, j++){
      payload_data[j] = buf[i]; 
    }

    /* Write payload data to file! */
    int w = fwrite(payload_data, 1, mlen, f_recv);
    cout << "writing: " << buf << endl;
    
    if(mlen<0){
      perror("recvfrom");
      exit(1);
    }
    
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
    
    /*  Send ACK to client */
    char resp[BSIZE];
    memset(resp, 0, BSIZE);
    resp[0] = '2'; // 2 = ACK
    resp[1] = sender_seq_num; // 1 or 0
    //    int sent = sendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
    int sent = nsendto(sd, resp, strlen(resp), 0, (struct sockaddr *)&cad, fromlen);
    if (sent < 0) cout << "ERROR\n";
    my_seq_num = advance_seq_num(my_seq_num);
  }

  // Handle termination! 
  fclose(f_recv);
}
