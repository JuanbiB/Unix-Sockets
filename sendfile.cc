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

#include "nsendto.c"

#define PORTNUMBER       8080           
#define MAXBUFFER        40

using namespace std;


int ctoi(char * num) {
  string s(num);
  return stoi(s);
}

int main(int argc, char* argv[]) {
  char* host_name; // Host to send file to. 
  int port_numer; // Port number of host.
  char* file_name; // File to send.
  int drop_p; // Probablilty of dropping package.
  int byte_err_p; // Probability of there being a byte error.

  if (argc < 4) {
    cout << "Not enough arguments!\n";
    exit(1);
  }

  // For testing
  if (strcmp(argv[1], "test")) {
  }
  
  else if (argc == 5) {
    host_name = argv[1];
    port_numer = ctoi(argv[2]);
    file_name = argv[3];
    drop_p = ctoi(argv[4]);
  }
  else { // just fail for now
    cout << "Nope\n";
    exit(1);
 }

  // 1) Create socket with hostname and post.
  // 2) Bind to it.
  // 3) Start reading file and sending it over.
  
  return 0;
}
