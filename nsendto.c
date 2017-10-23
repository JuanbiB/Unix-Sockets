#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

static double dropProbability=0.0;
static double byteProbability=0.0;
static int initialized = 0;
static int cleanused = 0;

void ninit(double dropP, double byteP){
  dropProbability = dropP;
  byteProbability = byteP;
  initialized = 1;
  cleanused = 0;
}

ssize_t nsendto(int sd, void*msg, ssize_t len, int flags,
		struct sockaddr*saddr, socklen_t tolen){
  int i;
  if(cleanused)
    return -1;
  else {
    double x = drand48();
    if(x>dropProbability){
      char* gmsg = (char*)malloc(len);
      for(i=0;i<len;i++){
	gmsg[i] = ((char*)msg)[i];
	x = drand48();
	if(x<byteProbability){
	  gmsg[i] ^= ((i*57) % 256);
	}
      }
      return sendto(sd,gmsg,len,flags,saddr,tolen);
    }
    else
      return len;
  }
}

ssize_t nrecvfrom(int sd, void*buf, size_t len, int flags,struct sockaddr *saddr,
		  socklen_t *fromlen){
  return recvfrom(sd,buf,len,flags,saddr,fromlen);
}

