// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#define MILLISEC 1000000
#define TIMEOUT  50000
#define TIMEWAIT 10000

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
  typedef int status;
  typedef unsigned long long lockid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
};

#endif 
