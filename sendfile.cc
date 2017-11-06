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

int build_payload(char* payload, FILE* f_send, int sender_seq_num){
  memset(payload, 0, MAXLINE);
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
    if (buffer[i] != '\0') {
      size++;
    }
    else {
      return size;
    }
  }
  return size;
}

void log(string s) {
  cout << s << endl;
}

int diff_between(int a, int b){
  int diff = 0;
  while (a != b){
    diff++;
    a = (a+1)%8;
  }
  return diff;
}

void print_frames(char frames[7][MAXLINE]){
  cout << "\n---------------------\n";
  for (int i = 0; i < 7; i++){
    for (int j = 0; j < MAXLINE; j++){
      cout << frames[i][j];
    }
  }
  cout << "\n---------------------\n";
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
  int frame_len[FRAME_NUM];
  
  for (int i = 0; i < FRAME_NUM; i++){
    memset(frames[i], 0, MAXLINE);
  }
  memset(frame_len, 0, MAXLINE);

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

  int seq_max = 7;
  

  /* Initial send to get us started. */
  for (int i = 0; i < 7; i++){
    memset(payload, 0, MAXLINE);
    int read = build_payload(payload, f_send, sender_seq_num);
    read_so_far += read;

    int sqq = sender_seq_num % FRAME_NUM;
    cout << "Sending frame number: " << sqq << endl;
    sender_seq_num++;

    copy_buffer(payload, read+4, frames[i]);
    frame_len[i] = read+4;

    /* Send a message to the server. */
    int sent_bytes = 0;
    if((sent_bytes = sendto(sd, frames[i], read+4, 0,
			    (struct sockaddr*)&sad, sizeof(sad))) < 0) {
      perror("sendto");
      exit(1);
    }
  }

  int sent_datagrams = 7;
  int resends = 0;
  int failed_acks = 0;
  int num_time_outs = 0;

  int Rn; /* Request number. Received from receiver. */
  int Sb = 0; /* Sequence base. */
  int Sm = 6; /* I just sent 0 - 6*/

  while (true) {
    bool timed_out = time_out(sd);
    log("receiving...");

    if (!timed_out) {
      /* Receive packet. */
      if((nbytes=recvfrom(sd, recvline, MAXLINE, 0,
			  (struct sockaddr*)&sad, &fromlen)) < 0) {
	perror("recvfrom");
	exit(1);
      }

      uint16_t recv_crc = getCRC2(recvline, nbytes);
      if (recv_crc != 0) {
	failed_acks++;
	continue;
      }
	
      int Rn = (recvline[1] - '0');
      cout << "Request number: " << Rn << endl;
      
      /* When an expected ACK is received, advance window, send more frame(s).*/
      /* If the ack is anywhere between the expected frames, we're good.
	 E.g. If I send |1,2,3,4,5,6| and get ACK 3, then we can move the window 
	 forward to |3,4,5,6,7,1,0|. But I shouldn't get 0, because how could it request
	 it if I haven't even sent 7... */
      if (Rn != Sb) {
	/* The base is now the last requested number..*/
	int slide_amount = diff_between(Sb, Rn);
	cout << "Slide amount: " << slide_amount << endl;

	/* Slide the window to the left. */
	for (int i = 0; i < 7; i++){
	  for (int j = 0; j < MAXLINE; j++){
	    frames[i][j] = frames[i+slide_amount][j];
	  }
	}
	/* Shifting frame length values. */
	for (int i = 0; i < 7; i++){
	  frame_len[i] = frame_len[i+slide_amount];
	}
	/* Clear data for new paylods and frame lengths.  */
	for (int i = (7-slide_amount); i < 7; i++){
	  memset(frames[i], 0, MAXLINE);
	  frame_len[i] = 0;
	}

	/* Calculate new sequence base and sequence max. */
	Sb = Rn;
	int last_max = Sm;
	Sm = (Sm + slide_amount) % 8;

	/* We build the payload with the requested sequence number. */
	int curr = (last_max + 1) % 8;
	int write_spot = (7 - slide_amount);
	cout << "Advancing window...\n";
	while (true){
	  int read = build_payload(payload, f_send, curr);

	  if (read > 0) {
	    int sent_bytes = 0;

	    if((sent_bytes = nsendto(sd, payload, read+4, 0,
				     (struct sockaddr*)&sad, sizeof(sad))) < 0) {
	      perror("sendto");
	      exit(1);
	    }
	    sent_datagrams++;
	    /* Overwite the spot where the last ack'd frame was. */
	    copy_buffer(payload, read+4, frames[write_spot]);
	    frame_len[write_spot] = read+4;
	  }
	  else {
	    timed_out = time_out(sd);
	    if (timed_out) break;
	  }
	  curr = (curr + 1) % 8;
	  if (curr == ((Sm + 1) % 8)) break;
	  write_spot++;
	}
      }
      /* When an unexpected ACK is received, 
	 it can be ignored or used as a trigger to resend. */
      else {
	log("Unexpected ACK, ignoring.\n");
      }
    }
    /* If a timeout occurs without an ACK, resend all unackowledged frames. */
    else {
      cout << "Timeout...\n";
      num_time_outs++;
      if (frames[0][0] == 0) break;
      for (int i = 0; i < 7; i++){
	int sent_bytes = 0;
	/* We only send if that frame isn't null. */
	if (frames[i][0] != 0) {
	  resends++;
	  if((sent_bytes = nsendto(sd, frames[i], frame_len[i], 0,
				  (struct sockaddr*)&sad, sizeof(sad))) < 0) {
	    perror("sendto");
	    exit(1);
	  }
	}
      }
    }
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
  
  cout << "\n<----- Summary ----->\n";
  cout << "File size: " << file_size << endl;
  cout << "Datagrams sent: " << sent_datagrams << endl;
  cout << "Resends: " << resends << endl;
  cout << "ACKs that failed CRC: " << failed_acks << endl;
  cout << "Timeouts: " << num_time_outs << endl;
  cout << endl;

  /* Close the socket. */
  fclose(f_send);
  close(sd);
}
