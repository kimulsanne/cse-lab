//
// Lock demo
//

#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <arpa/inet.h>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

std::string dst;
lock_client *lc;

int
main(int argc, char *argv[])
{
  int r;

  if(argc != 2){
    fprintf(stderr, "Usage: %s [host:]port\n", argv[0]);
    exit(1);
  }

  dst = argv[1];
  lc = new lock_client(dst);
  r = lc->acquire(1);
  int i = 0;
  while(i<5){
  	lc->heartbeat(1);
	sleep(1);
	++i;
  }
  printf ("stat returned %d\n", r);
}
