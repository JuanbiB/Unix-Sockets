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
#include "types.cc"
#include "crc_generator.cc"

#define MAXBUFFER        40
#define MAXLINE          40
#define TIMEOUT          100
#define FRAME_NUM        7
using namespace std;

int find_file_size (FILE* ptr) {
  fseek(ptr, 0, SEEK_END);
  int size = ftell(ptr);
  rewind(ptr);
  return size;
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

/* Makes sure that the message is either an ACK with
   correct sequence number or a FINACK. */
bool handle_response(char* recvline, char sender_seq_num) {
  char msg_type = recvline[0];
  char recv_seq_num = recvline[1];

  if ((msg_type == '2' && recv_seq_num == sender_seq_num)
      || msg_type == '5') {
    return true;
  }

  cout << "ACK frame recv with incorrect seq number.\nShould send again.\n";
  return false;
}

void resend_message(int sd, char* payload, int len, struct sockaddr* sad,
		    bool timed_out, char* recvline) {
  while (timed_out) {
    if((nsendto(sd, payload, len, 0,
	       sad, sizeof(struct sockaddr))) < 0) {
      perror("sendto");
      exit(1);
    }
    timed_out = time_out(sd);
  }
  socklen_t size = sizeof(struct sockaddr_in);

  memset(recvline, 0, len);
  if((recvfrom(sd, recvline, 2, 0,
	       sad, &size)) < 0) {
    perror("recvfrom");
    exit(1);
  }
}

int build_payload(char* payload, FILE* f_send, int sender_seq_num){
  payload[0] = '1'; // DATA
  payload[1] = sender_seq_num + '0';
  /* Reading in data from file. */
  char temp[MAXLINE-4];
  memset(temp, 0, MAXLINE-4);
  int read = fread(temp, 1, MAXLINE-4, f_send);
  
  /* Copy DATA into payload. */
  for (int i = 2, j = 0; j < read; i++, j++) {
    payload[i] = temp[j];
  }

  /* Calculating CRC-16 code! */
  uint16_t crc = getCRC2(payload, read+2);
  //printf("CRC: 0x%x\n", crc);
  uint8_t left = crc  >> 8;
  uint8_t right = crc & 0xFF;

  payload[2+read] = left;
  payload[2+read+1] = right;

  return read;
}

void copy_buffer(char* source, int source_len, char* destination){
  memset(destination, 0, MAXLINE);
  for (int i = 0; i < source_len; i++){
    destination[i] = source[i];
  }
}

int find_size(char* buffer) {
  int size = 0;
  for (int i = 0; i < MAXLINE; i++){
    if (buffer[i] != 0) {
      size++;
    }
    else {
      return size;
    }
  }
  return size;
}

int main(int argc, char* argv[]) {
  char* host_name; // Host to send file to. 
  int port_number; // Port number of host.
  char* file_name; // File to send.
  double drop_p; // Probablilty of dropping package.
  double byte_err_p; // Probability of there being a byte error.


  if (argc > 5) {
    host_name = argv[1];
    port_number = atoi(argv[2]);
    file_name = argv[3];
    char* ptr;
    drop_p = strtod(argv[4], &ptr);
    byte_err_p = strtod(argv[5], &ptr);
    
  }
  else {
    cout << "Not enough arguments.\n";
    exit(1);
  }

  cout << byte_err_p << endl;
  cout << drop_p << endl;
  ninit(drop_p, byte_err_p);
  // 1) Create socket with hostname and post.
  // 2) Bind to it.
  // 3) Start reading file and sending it over.
  struct  hostent  *ptrh;  /* pointer to a host table entry       */
  struct  sockaddr_in sad; /* structure to hold an IP address     */
  struct  sockaddr_in cad; /* structure to hold an IP address     */
  int     sd;              /* socket descriptor                   */
  socklen_t fromlen = sizeof(sad);
  int     nbytes;          /* number of bytes in reply message    */

  char recvline[MAXLINE]; /* receive buffer                    */
  char payload[MAXLINE]; /* send buffer    */
  char frames[FRAME_NUM][MAXLINE];
  for (int i = 0; i < FRAME_NUM; i++){
    memset(frames[i], 0, MAXLINE);
  }

  /*  Set up address for echo server  */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;         /* set family to Internet     */
  sad.sin_port = htons((u_short)port_number); /* convert port number to network byte order */

  /*  Set up address for local socket  */
  memset((char *)&cad,0,sizeof(sad)); /* clear sockaddr structure */
  cad.sin_family = AF_INET;         /* set family to Internet     */
  cad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address   */
  cad.sin_port = 0;  /* use any available port */

  /* Check host argument and assign host name. */
  string host(host_name);

  /* Convert host name to equivalent IP address and copy to sad. */
  if((ptrh=gethostbyname(host.c_str())) ==0 ) {
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
  if(bind(sd, (struct sockaddr *)&cad, sizeof(cad)) < 0) {
    perror("bind");
    exit(1);
  }

  if((ptrh = gethostbyaddr((char*)&cad.sin_addr, sizeof(struct sockaddr_in),AF_INET))==0) {
    printf("%s port %d\n", inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
  }
  else {
    printf("%s/%s port %d\n", ptrh->h_name,inet_ntoa(cad.sin_addr),ntohs(cad.sin_port));
  }

  /* Main loop.  Repeatedly get data from stdin, write it to
     the socket, then read data from socket and write it to stdout */
  FILE* f_send = fopen(file_name, "r");
  if (!f_send) {
    cout << "Couldn't open file" << endl;
    exit(1);
  }
  int file_size = find_file_size(f_send);
  int read_so_far = 0;

  int sender_seq_num = 0;
  int seq_base = 0;
  int seq_max = 7;
  
  while (true) {
    /* Initial send to get us started. */
    for (int i = seq_base; i < seq_max; i++){
      memset(payload, 0, MAXLINE);
      int read = build_payload(payload, f_send, sender_seq_num);
      read_so_far += read;

      int sqq = sender_seq_num % FRAME_NUM;
      cout << "Sending frame number: " << sqq << endl;
      sender_seq_num++;

      copy_buffer(payload, read+4, frames[i]);

      /* Send a message to the server. */
      int sent_bytes = 0;
      if((sent_bytes = nsendto(sd, frames[i], read+4, 0,
			       (struct sockaddr*)&sad, sizeof(sad))) < 0) {
	perror("sendto");
	exit(1);
      }
    }
    
    int expected_seq = 0;
    int total_received_written = 0;
    int last_requested = 0;
    while (true) {
      bool timed_out = time_out(sd);
      if (!timed_out) {
	/* Receive packet. */
	if((nbytes=recvfrom(sd, recvline, MAXLINE, 0,
			    (struct sockaddr*)&sad, &fromlen)) < 0) {
	  perror("recvfrom");
	  exit(1);
	}
	
	int received_seq = (recvline[1] - '0') % FRAME_NUM;
	cout << "received seq: " << received_seq << endl;
	last_requested = received_seq;

	/* When an expected ACK is received, advance window, send more frame(s).*/
	if (received_seq == seq_base) {
	  cout << "Received wanted sequence num: " << received_seq << endl;	  
	  int read = build_payload(payload, f_send, received_seq);
	  seq_base = (seq_base+1) % 7; 
	  if (read > 0) {

	    int sent_bytes = 0;
	    if((sent_bytes = nsendto(sd, payload, read+4, 0,
				     (struct sockaddr*)&sad, sizeof(sad))) < 0) {
	      perror("sendto");
	      exit(1);
	    }

	    if (seq_max > 0){
	      copy_buffer(payload, read+4, frames[seq_max-1]);
	    } else {
	      copy_buffer(payload, read+4, frames[6]);
	    }

	    /* Slide window to the right. Wraps around. */
	    seq_max = (seq_max + 1) % 7;

	  } else {
	    timed_out = time_out(sd);
	    if (timed_out) break;
	  }

	}
	/* When an unexpected ACK is received, it can be ignored or used as a trigger to resend.*/
	else {
	  cout << "Ignoring sequence num: " << received_seq << endl;
	  cout << "My base is: " << seq_base << endl;
	}
      }
      /* If a timeout occurs without an ACK, resend all unackowledged frames. */
      else {
	/* All 'unacknowledged' frames. How much do I get from the buffer and how much do I read in? */	
	cout << "Timeout.. Resending. \n";
	/* This is all wrong! */
	int sent_bytes = 0;
	cout << "last requested: " << last_requested << endl;
	if((sent_bytes = nsendto(sd, frames[last_requested], find_size(frames[last_requested]), 0,
				(struct sockaddr*)&sad, sizeof(sad))) < 0) {
	  perror("sendto");
	  exit(1);

	}
      }
    }
    break;
  }
  
  /* Handle termination of file transfer. */
  int sent_bytes;
  char end[MAXLINE];
  memset(end, 0, MAXLINE);
  int fin_seq = sender_seq_num % FRAME_NUM;
  end[0] = '4';
  end[1] = fin_seq + '0';
  /* Send FIN.  */
  if((sent_bytes = sendto(sd, end, 2, 0,
			  (struct sockaddr*)&sad, sizeof(sad))) < 0) {
    perror("sendto");
    exit(1);
  }
  socklen_t size = sizeof(struct sockaddr);
  int rec;
  /* Receive FINACK.  */
  if((rec = recvfrom(sd, recvline, MAXLINE, 0,
		     (struct sockaddr*)&sad, &size)) < 0) {
    perror("recvfrom");
    exit(1);
  }
  cout << "Received at end: " << recvline[0] << endl;
  if (true) {
    int sent_bytes;
    char end[MAXLINE];
    memset(end, 0, MAXLINE);
    /* Send ACK.  */
    end[0] = '2'; 
    if((sent_bytes = sendto(sd, end, 2, 0,
			    (struct sockaddr*)&sad, sizeof(sad))) < 0) {
      perror("sendto");
      exit(1);
    }
  }
  cout << "File size: " << file_size << endl;  
  /* Close the socket. */
  fclose(f_send);
  close(sd);
}
