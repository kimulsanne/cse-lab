#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include <pthread.h>
#include <signal.h>

struct LogState;

class yfs_client {
  extent_client *ec;
  lock_client *lc;
  pthread_mutex_t log_mutex;
  int yfs_version;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct syminfo{
  	unsigned long long size;
  	unsigned long atime;
	unsigned long mtime;
	unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  static bool findN(const char*, const std::string&);
  int parse_log(std::vector<LogState> &, int);
  void write_log(const std::string &);
  void recoverTo(std::vector<LogState> &);

 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  
  /** you may need to add symbolic link related methods here.*/
  int symlink(const char*, inum, const char*, inum&);
  int readlink(inum, std::string&);
  int getsym(inum, syminfo &);

  void checkpoint();
  void pre_version();
  void next_version();
};

enum Oper{ROOTDIR, CREATE, MKDIR, WRITE, UNLINK, COMMIT, CHECKPOINT};

struct LogState{
	Oper op;
	yfs_client::inum ino;
	yfs_client::inum parent;
	std::string dir;
	std::string content;
};

#endif 
