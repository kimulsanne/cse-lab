// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
   	ec = new extent_client(extent_dst);
	lc = new lock_client(lock_dst);
	pthread_mutex_init(&log_mutex, NULL);
	yfs_version = 0;
	std::vector<LogState> logs;
	if(parse_log(logs, -1) == 0){
		lc->acquire(1);
	    if (ec->put(1, "") != extent_protocol::OK)
    	    printf("error init root dir\n"); // XYB: init root dir

		std::string log = "rootdir 1\ncommit 1\n";
		write_log(log);
		lc->release(1);
	}
	yfs_version++;
}

void yfs_client::recoverTo(std::vector<LogState> &logs){
	pthread_mutex_lock(&log_mutex);
	ec->cleanAllNodes(1);
	int logCnt = logs.size();

	for(int i= 0; i< logCnt; ++i){
		switch(logs[i].op){
		case ROOTDIR:
			ec->set_ino(1, extent_protocol::T_DIR, std::string(""));
			break;
		case MKDIR:
			ec->set_ino(logs[i].ino, extent_protocol::T_DIR, std::string(""));
			ec->set_ino(logs[i].parent, extent_protocol::T_DIR, logs[i].dir);
			break;
		case CREATE:
			ec->set_ino(logs[i].ino, extent_protocol::T_FILE, std::string(""));
			ec->set_ino(logs[i].parent, extent_protocol::T_DIR, logs[i].dir);
			break;
		case WRITE:
			ec->put(logs[i].ino, logs[i].content);
			break;
		case UNLINK:
			ec->set_ino(logs[i].ino, 0, std::string(""));
			ec->set_ino(logs[i].parent, extent_protocol::T_DIR, logs[i].dir);
			break;
		case CHECKPOINT:
			break;
		default:
			break;
		}
	}
	pthread_mutex_unlock(&log_mutex);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

	lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
		lc->release(inum);
        printf("error getting attr\n");
        return false;
    }
	lc->release(inum);

    if (a.type == extent_protocol::T_FILE) {

        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

int
yfs_client::symlink(const char* link, inum parent, const char* name, inum& ino_out){	
	int r = OK;
	bool found;
	std::string parentDir;

	lc->acquire(parent);

	ec->get(parent, parentDir);
	found = findN(name, parentDir);
	if(found){
		lc->release(parent);
		r = EXIST;
	}else{	
		ec->create(extent_protocol::T_SYM, ino_out);

		lc->acquire(ino_out);
		ec->put(ino_out, std::string(link));
		lc->release(ino_out);

		parentDir += filename(ino_out);
		parentDir += "/";
		parentDir += std::string(name);
		parentDir += "/";
	
		char dir_size_c[11];
		char link_length_c[11];
		sprintf(dir_size_c, "%d", parentDir.size());
		sprintf(link_length_c, "%d", std::string(link).size());
		std::string log = "symlink "+ filename(ino_out)+" under "+filename(parent)+" dir: "+std::string(dir_size_c)+" "+parentDir+" link: "+std::string(link_length_c)+" "+std::string(link)+"\ncommit "+filename(ino_out)+"\n";
		write_log(log);


		ec->put(parent, parentDir);
		lc->release(parent);
		
		
	}
	return r;
}

int
yfs_client::readlink(inum ino, std::string& link){
	int r = OK;

	lc->acquire(ino);
	ec->get(ino, link);
	lc->release(ino);
	return r;
}


int yfs_client::getsym(inum inum, syminfo &sin){
	int r = OK;
    printf("getsym %016llx\n", inum);
    extent_protocol::attr a;

	lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("getsim %016llx -> sz %llu\n", inum, sin.size);

release:
	lc->release(inum);
    return r;
}


bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    if(isfile(inum)) return false;
	else{
		extent_protocol::attr a;

		lc->acquire(inum);		
		if (ec->getattr(inum, a) != extent_protocol::OK) {
		 	printf("error getting attr\n");
			lc->release(inum);
			return false;
		}
		lc->release(inum);
		if (a.type == extent_protocol::T_DIR) {
			printf("isdir: %lld is a dir\n", inum);
			return true;
		}else{
			printf("issym: %lld is a sym\n", inum);
			return false;
		}
	}
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;

    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
	lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;

    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
	lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
	//printf("\n\n\nsetattr:\nsize:%d\n", size);
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
	std::string contents;

	lc->acquire(ino);
	ec->get(ino, contents);
	if(contents.length() > size)
		contents.erase(size);
	else if(contents.length() < size)
		contents.resize(size);
	ec->put(ino, contents);
	 
	lc->release(ino);
    return r;
}
int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
	bool found;
	std::string parentDir;

	lc->acquire(parent);

	//get parent dir
	ec->get(parent, parentDir);
	found = findN(name, parentDir);
	if(found){
		lc->release(parent);
		r = EXIST;
	}else{
		//add new file to parent dir
		ec->create(extent_protocol::T_FILE, ino_out);

		//printf("\n\n\ninum:%lld\nparentDir:%s\n\n\n", ino_out, parentDir.c_str());
		parentDir += filename(ino_out);
		parentDir += "/";
		parentDir += std::string(name);
		parentDir += "/";
		
		//logging
		char dir_size_c[11];
		sprintf(dir_size_c, "%d", parentDir.size());
		std::string log = "create "+ filename(ino_out)+" under "+filename(parent)+" dir: "+std::string(dir_size_c)+" "+parentDir+"\ncommit "+filename(ino_out)+"\n";
		write_log(log);

		ec->put(parent, parentDir);
		//printf("\n\n\ninum:%lld\nparentDir:%s\n\n\n", ino_out, parentDir.c_str());
		lc->release(parent);
	}
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent information.
     */
	bool found;
	std::string parentDir;

	lc->acquire(parent);

	//get parent dir
	ec->get(parent, parentDir);
	found = findN(name, parentDir);
	if(found){
		lc->release(parent);
		r = EXIST;
	}else{
		//add new file to parent dir
		ec->create(extent_protocol::T_DIR, ino_out);
		parentDir += filename(ino_out);
		parentDir += "/";
		parentDir += std::string(name);
		parentDir += "/";

		//logging
		char dir_size_c[11];
		sprintf(dir_size_c, "%d", parentDir.size());
		std::string log = "mkdir "+ filename(ino_out)+" under "+filename(parent)+" dir: "+std::string(dir_size_c)+" "+parentDir+"\ncommit "+filename(ino_out)+"\n";
		write_log(log);

		ec->put(parent, parentDir);
		lc->release(parent);
	}
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
	found = false;
	std::list<dirent> dirList;
	r = readdir(parent, dirList);
	std::list<dirent>::iterator iter;
	for(iter = dirList.begin(); iter != dirList.end(); ++iter){
		if(iter->name == std::string(name)){
			found = true;
			ino_out = iter->inum;
			break;
		}
	}
	return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
	std::string dirStr;

	lc->acquire(dir);
	ec->get(dir, dirStr);
	lc->release(dir);

	//2d(dirStr, list);
	
	int pOld = 0, pNew = 0;
	inum ino;
	std::string inoStr, filename;
	dirent tmpDirent;
	while(true){
		
		pNew = dirStr.find('/', pOld);
		if(pNew == -1) break;
		inoStr = dirStr.substr(pOld, pNew-pOld);
		ino = n2i(inoStr);
		pOld = pNew+1;
		
		pNew = dirStr.find('/', pOld);
		filename = dirStr.substr(pOld, pNew-pOld);
		pOld = pNew+1;
		
		tmpDirent.inum = ino;
		tmpDirent.name = filename;
		list.push_back(tmpDirent);
	}
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
	std::string contents;

	lc->acquire(ino);
	ec->get(ino, contents);
	lc->release(ino);
	if(off > contents.length()) data = "";
	else data = contents.substr(off, size);
	//printf("\n\n\nread:\nino:%d\noff:%d\nrequest length:%d\nactual length:%d\n%s\n\n\n", ino,off,size,data.length(),data.c_str());
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
	std::string contents;
	std::string dataStr = std::string(data, size);

	lc->acquire(ino);

	ec->get(ino, contents);
	int contentLength = contents.length();
	bytes_written = 0;
	if(off > contentLength){
		contents.resize(off);
		contents += dataStr;
		if(contents.size() > off+size)
			contents.erase(off+size);
		bytes_written = size + off - contentLength;
	}else if(off+size >= contentLength){
		if(off < contents.size())
			contents.erase(off);
		contents += dataStr;
		if(contents.size() > off+size)
			contents.erase(off+size);
		bytes_written = size;
	}else{
		bytes_written = size;
		contents.replace(off, size, dataStr.substr(0,size));
	}

	char buf_size_c[11];
	sprintf(buf_size_c, "%d", contents.size());
	std::string log = "write "+ filename(ino)+" content: "+std::string(buf_size_c)+" "+contents+"\ncommit "+filename(ino)+"\n";
	write_log(log);

	ec->put(ino, contents);
	lc->release(ino);	
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
	bool found;
	yfs_client::inum ino;
	r = lookup(parent, name, found, ino);
	if(found){
		std::string parentDir, inoStr;

		lc->acquire(parent);
		lc->acquire(ino);
		ec->get(parent, parentDir);
	
		
		unsigned int pOld = 0, pNew = 0;
		while(pOld < parentDir.size()){
			
			pNew = parentDir.find('/', pOld);
			inoStr = parentDir.substr(pOld, pNew-pOld);
			if(n2i(inoStr) == ino){
				
				pNew = parentDir.find('/', pNew+1);
				parentDir.erase(pOld, pNew-pOld+1);
				break;
			}
			pOld = pNew+1;
			
			pNew = parentDir.find('/', pOld);
			pOld = pNew+1;
		}

		
		char dir_size_c[11];
		sprintf(dir_size_c, "%d", parentDir.size());
		std::string log = "unlink "+ filename(ino)+" under "+filename(parent)+" dir: "+std::string(dir_size_c)+" "+parentDir+"\ncommit "+filename(ino)+"\n";
		write_log(log);

		ec->put(parent, parentDir);
		ec->remove(ino);		
		lc->release(parent);
		lc->release(ino);
	}else
		r = NOENT;
    return r;
}

bool yfs_client::findN(const char* name, const std::string& parentDir){
	bool found = false;
	std::list<dirent> dirList;
	
	int pOld = 0, pNew = 0;
	inum ino;
	std::string inoStr, filename;
	dirent tmpDirent;
	while(true){	
		
		pNew = parentDir.find('/', pOld);
		if(pNew == -1) break;
		inoStr = parentDir.substr(pOld, pNew-pOld);
		ino = n2i(inoStr);
		pOld = pNew+1;
		
		pNew = parentDir.find('/', pOld);
		filename = parentDir.substr(pOld, pNew-pOld);
		pOld = pNew+1;
		
		tmpDirent.inum = ino;
		tmpDirent.name = filename;
		dirList.push_back(tmpDirent);
	}

	std::list<dirent>::iterator iter;
	for(iter = dirList.begin(); iter != dirList.end(); ++iter){
		if(iter->name == std::string(name)){
			found = true;
			break;
		}
	}
	return found;
}


int yfs_client::parse_log(std::vector<LogState> &logs, int version){

	pthread_mutex_lock(&log_mutex);
	std::ifstream ist;
	ist.open(LOG_NAME, std::ios::binary);
	if(!ist){
		pthread_mutex_unlock(&log_mutex);
		return 0;
	}
	
	yfs_version = 0;
	std::list<int> checkpoints;
	LogState log;
	std::string buf;
	char tmpCh;
	int tmpLength;
	while(ist >> buf){
		log.dir = "";
		log.content = "";
		if(buf == "rootdir"){
			ist>>log.ino;
			ist>>buf;
			if(buf != "commit")
				break;
			ist>>log.ino;
			log.op = ROOTDIR;
			logs.push_back(log);
		}else if(buf == "mkdir"){
			ist>>log.ino;
			ist>>buf; 
			ist>>log.parent;
			ist>>buf; 
			ist>>tmpLength;
			ist.get(tmpCh); 
			for(int i= 0; i< tmpLength; ++i){
				ist.get(tmpCh);
				log.dir += tmpCh;
			}
			ist>>buf;
			if(buf != "commit")
				break;
			ist>>log.ino;
			log.op = MKDIR;
			logs.push_back(log);
		}else if(buf == "create"){
			ist>>log.ino;
			ist>>buf;  
			ist>>log.parent;
			ist>>buf;  
			ist>>tmpLength;
			ist.get(tmpCh);  
			for(int i= 0; i< tmpLength; ++i){
				ist.get(tmpCh);
				log.dir += tmpCh;
			}
			ist>>buf;
			if(buf != "commit")
				break;
			ist>>log.ino;
			log.op = CREATE;
			logs.push_back(log);
		}else if(buf == "write"){
			ist>>log.ino;
			ist>>buf;  
			ist>>tmpLength;
			ist.get(tmpCh);  
			for(int i= 0; i< tmpLength; ++i){
				ist.get(tmpCh);
				log.content += tmpCh;
			}
			ist>>buf;
			if(buf != "commit")
				break;
			ist>>log.ino;
			log.op = WRITE;
			logs.push_back(log);		
		}else if(buf == "unlink"){
			ist>>log.ino;
			ist>>buf;  
			ist>>log.parent;
			ist>>buf;  
			ist>>tmpLength;
			ist.get(tmpCh); 
			for(int i= 0; i< tmpLength; ++i){
				ist.get(tmpCh);
				log.dir += tmpCh;
			}
			ist>>buf;
			if(buf != "commit")
				break;
			ist>>log.ino;
			log.op = UNLINK;
			logs.push_back(log);
		}else if(buf == "checkpoint"){
			ist>>buf;
			if(buf != "commit")
				break;
			log.op = CHECKPOINT;
			logs.push_back(log);
			yfs_version++;
			if(yfs_version == version)
				break;
		}
	}
	ist.close();
	pthread_mutex_unlock(&log_mutex);
	if(logs.size() == 0) return 0;
	return 1;
}

void yfs_client::write_log(const std::string &buf){
	pthread_mutex_lock(&log_mutex);
	std::ofstream ost;
	ost.open(LOG_NAME, std::ios::binary|std::ios::app);
	ost<<buf;
	ost.close();
	pthread_mutex_unlock(&log_mutex);
}


void yfs_client::checkpoint(){
	std::string log = "checkpoint\ncommit\n";
	write_log(log);
	yfs_version++;	
}

void yfs_client::pre_version(){
	std::vector<LogState> logs;
	yfs_version--;
	parse_log(logs, yfs_version);
	recoverTo(logs);
}

void yfs_client::next_version(){
	std::vector<LogState> logs;
	yfs_version++;
	parse_log(logs, yfs_version);
	recoverTo(logs);
}
