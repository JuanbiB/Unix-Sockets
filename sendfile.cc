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

#include "nsendto.c"

#define PORTNUMBER       5193           
#define MAXBUFFER        40
#define MAXLINE 40

using namespace std;

int find_file_size (FILE* ptr) {
  fseek(ptr, 0, SEEK_END);
  int size = ftell(ptr);
  rewind(ptr);
  return size;
}

int main(int argc, char* argv[]) {
  char* host_name; // Host to send file to. 
  int port_number; // Port number of host.
  char* file_name; // File to send.
  int drop_p; // Probablilty of dropping package.
  int byte_err_p; // Probability of there being a byte error.

  if (argc == 5) {
    host_name = argv[1];
    port_number = atoi(argv[2]);
    file_name = argv[3];
    drop_p = atoi(argv[4]);
  }
  else { 
    cout << "No args. \n";
  }

  // 1) Create socket with hostname and post.
  // 2) Bind to it.
  // 3) Start reading file and sending it over.
  struct  hostent  *ptrh;  /* pointer to a host table entry       */
  struct  sockaddr_in sad; /* structure to hold an IP address     */
  struct  sockaddr_in cad; /* structure to hold an IP address     */
  int     sd;              /* socket descriptor                   */
  socklen_t     fromlen = sizeof(sad);
  int     nbytes;          /* number of bytes in reply message    */
  string host;           /* pointer to host name                */
  char payload[MAXLINE]; /* send buffer                       */
  char recvline[MAXLINE]; /* receive buffer                    */

  /*  Set up address for echo server  */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;         /* set family to Internet     */
  sad.sin_port = htons((u_short)PORTNUMBER); /* convert port number to 
						network byte order */

  /*  Set up address for local socket  */

  memset((char *)&cad,0,sizeof(sad)); /* clear sockaddr structure */
  cad.sin_family = AF_INET;         /* set family to Internet     */
  cad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address   */
  cad.sin_port = 0;  /* use any available port */

  /* Check host argument and assign host name. */

  if (argc > 1) {
    host = argv[1];         /* if host argument specified   */
  } else {
    host = "localhost";
  }

  /* Convert host name to equivalent IP address and copy to sad. */
  if((ptrh=gethostbyname(host.c_str()))==0){
    fprintf(stderr,"invalid host: %s\n", host.c_str());
    exit(1);
  }
  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  /* Create a datagram socket */
  sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sd < 0) {
    perror("socket creation");
    exit(1);
  }

  /* Bind the socket to any local address */
  if(bind(sd, (struct sockaddr *)&cad, sizeof(cad))<0){
    perror("bind");
    exit(1);
  }

  if((ptrh = gethostbyaddr((char*)&cad.sin_addr,
			   sizeof(struct sockaddr_in),AF_INET))==0){
    printf("%s port %d\n",
	   inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
  }
  else {
    printf("%s/%s port %d\n",
	  ptrh->h_name,inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
  }

  /* Main loop.  Repeatedly get data from stdin, write it to
     the socket, then read data from socket and write it to stdout */
  //  while (fgets(sendline,MAXLINE,stdin) != NULL) {
  
  // Open file and find it's size. 
  string file = "send.txt";
  FILE* f_send = fopen(file.c_str(), "r");
  if (!f_send) {
    cout << "Couldn't open file" << endl;
    exit(1);
  }
  int file_size = find_file_size(f_send);
  int read_so_far = 0;
  cout << "File size: " << file_size << endl;
  char sender_seq_num = '0';
  
  while (read_so_far < file_size) {
    memset(payload, 0, MAXLINE);
    
    payload[0] = '1'; // DATA
    payload[1] = sender_seq_num; // 1 or 0.
    
    char temp[MAXLINE-4];
    // Leave room for code, seq_num and CRC. 
    memset(temp, 0, MAXLINE-4);
       int read = fread(temp, 1, MAXLINE-4, f_send);
    
    read_so_far += read;
    // // Copy DATA into payload
    int i = 2;
    int count = 0;
    while (count <= read) {
      payload[i] = temp[count];
      i++; count++;
    }
    // Dummies to hold future CRC code.
    payload[read+2] = 'C';
    payload[read+3] = 'R';
    cout << payload << endl;
    /*  Send a message to the server  */
    int sent_bytes = 0;
    if((sent_bytes = sendto(sd,payload, read+4, 0,
			    (struct sockaddr*)&sad, sizeof(sad)))<0){
      perror("sendto");
      exit(1);
    }
    
    cout << "Sent: " << sent_bytes << " bytes.\n";
    if (read < MAXLINE-4) break;

    // Set up a pollfd structure
    struct pollfd pollstr;
    pollstr.fd = sd; // the file descriptor for the socket you are using
    pollstr.events = POLLIN; // the events on the descriptor that you want to poll for
    struct pollfd pollarr[1];
    pollarr[0] = pollstr;

    int n = poll(pollarr,1,500); // 1 = array size, 100 = timeout value in ms
    // Received, read and go ahead
    if(n > 0){
      if((nbytes=recvfrom(sd,recvline,strlen(payload),0,
			  (struct sockaddr*)&sad, &fromlen))<0){
	perror("recvfrom");
	exit(1);
      }
      char msg_type = recvline[0];
      char recv_seq_num = recvline[1];
      if (msg_type == '2' && recv_seq_num == sender_seq_num) {
	cout << "ACK frame recv  with correct seq num.\n";
      }
      else if (msg_type == '2' && recv_seq_num != sender_seq_num) {
	cout << "ACK frame recv with incorrect seq number.\n Should send again.\n";
      }
      // FIN ACK and all that bullshit.
      else {
	cout << "Non-ACK fame received\n";
      }
    }
    // Timeout!
    else if(n==0) {
      cout << "Timeout! I don't know how to handle!\n";
      exit(1);
    }
    else {
      cout << "Other error\n"; exit(1);
    }
    /* Change seq number to next one. */
    if (sender_seq_num == '1') { sender_seq_num = '0'; }
    else {sender_seq_num = '1';}
  }

  // Handle termination of file transfer. 
  int sent_bytes;
  char end[MAXLINE];
  memset(end, 0, MAXLINE);
  end[0] = '4';
  if((sent_bytes = sendto(sd,end, 1, 0,
			  (struct sockaddr*)&sad, sizeof(sad)))<0){
    perror("sendto");
    exit(1);
  }
  /* Close the socket. */
  fclose(f_send);
  close(sd);
}
