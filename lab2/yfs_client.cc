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

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

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

puts("\n\nsymlink\n\n");
	int r = OK;
	bool found;
	inum foundIno;
	lookup(parent, name, found, foundIno);
	if(found) r = EXIST;
	else{
		//get parent dir
		std::string parentDir;
		ec->get(parent, parentDir);
		std::list<dirent> dirList;
		r = readdir(parent, dirList);
		//add new file to parent dir
		ec->create(extent_protocol::T_SYM, ino_out);
		dirent tmpDirent;
		tmpDirent.name = std::string(name);
		tmpDirent.inum = ino_out;
		dirList.push_back(tmpDirent);
		//write back to parent dir
		parentDir = "";
		std::list<dirent>::iterator iter;
		for(iter = dirList.begin(); iter != dirList.end(); ++iter){
			parentDir += filename(iter->inum);
			parentDir += "/";
			parentDir += iter->name;
			parentDir += "/";
		}
		parentDir += "0/";
		ec->put(parent, parentDir);
		printf("parent:%s", parentDir.c_str());
		ec->put(ino_out, std::string(link));
	}
	return r;
}

int
yfs_client::readlink(inum ino, std::string& link){
	int r = OK;
	printf("\n\n\nreadlink\n\n\n");
	ec->get(ino, link);
	return r;
}


int yfs_client::getsym(inum inum, syminfo &sin){
	int r = OK;

    printf("getsym %016llx\n", inum);
    extent_protocol::attr a;
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
    return r;
}


bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    if(isfile(inum)) return false;
	else{
		extent_protocol::attr a;
		if (ec->getattr(inum, a) != extent_protocol::OK) {
		 	printf("error getting attr\n");
			return false;
		}
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
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
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
	std::string tmpBuf;
	ec->get(ino, tmpBuf);
	if(tmpBuf.length() > size)
		tmpBuf.erase(size);
	else if(tmpBuf.length() < size)
		tmpBuf.resize(size);
	ec->put(ino, tmpBuf);
	//printf("acturalSize:%d\n\n\n", tmpBuf.size());
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
	inum foundIno;
	lookup(parent, name, found, foundIno);
	if(found) r = EXIST;
	else{
		//get parent dir
		std::string parentDir;
		ec->get(parent, parentDir);
		std::list<dirent> dirList;
		r = readdir(parent, dirList);
		//add new file to parent dir
		ec->create(extent_protocol::T_FILE, ino_out);
		dirent tmpDirent;
		tmpDirent.name = std::string(name);
		tmpDirent.inum = ino_out;
		dirList.push_back(tmpDirent);
		//write back to parent dir
		parentDir = "";
		std::list<dirent>::iterator iter;
		for(iter = dirList.begin(); iter != dirList.end(); ++iter){
			parentDir += filename(iter->inum);
			parentDir += "/";
			parentDir += iter->name;
			parentDir += "/";
		}
		parentDir += "0/";
		ec->put(parent, parentDir);
		//printf("\n\n\ninum:%d\nparentDirSize:%d\n\n\n", ino_out, parentDir.size());
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
	inum foundIno;
	lookup(parent, name, found, foundIno);
	if(found) r = EXIST;
	else{
		//get parent dir
		std::string parentDir;
		ec->get(parent, parentDir);
		std::list<dirent> dirList;
		r = readdir(parent, dirList);
		//add new file to parent dir
		ec->create(extent_protocol::T_DIR, ino_out);
		dirent tmpDirent;
		tmpDirent.name = std::string(name);
		tmpDirent.inum = ino_out;
		dirList.push_back(tmpDirent);
		//write back to parent dir
		parentDir = "";
		std::list<dirent>::iterator iter;
		for(iter = dirList.begin(); iter != dirList.end(); ++iter){
			parentDir += filename(iter->inum);
			parentDir += "/";
			parentDir += iter->name;
			parentDir += "/";
		}
		parentDir += "0/";
		ec->put(parent, parentDir);
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
	std::string dirBuf, filename, inoStr;
	int ino;
	dirent tmpDirent;
	//get dir info from extent_client
	ec->get(dir, dirBuf);
	int pOld = 0, pNew = 0;
	while(true){
		//ino
		pNew = dirBuf.find('/', pOld);
		if(pNew == -1) break;
		inoStr = dirBuf.substr(pOld, pNew-pOld);
		ino = n2i(inoStr);
		if(ino == 0) break;
		pOld = pNew+1;
		//file name
		pNew = dirBuf.find('/', pOld);
		filename = dirBuf.substr(pOld, pNew-pOld);
		pOld = pNew+1;
		//push_back
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
	std::string tmpBuf;
	ec->get(ino, tmpBuf);
	if(off > tmpBuf.length()) data = "";
	else data = tmpBuf.substr(off, size);
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
	std::string tmpBuf;
	//data too small, fill with \0
//	std::string dataStr = std::string(data);
//	if(size > dataStr.size())
//		dataStr.resize(size, '\0');
	std::string dataStr = std::string(data, size);
	ec->get(ino, tmpBuf);
	int contentLength = tmpBuf.length();
	bytes_written = 0;
	if(off > contentLength){
		tmpBuf.resize(off);
		tmpBuf += dataStr;
		if(tmpBuf.size() > off+size)
			tmpBuf.erase(off+size);
		bytes_written = size + off - contentLength;
	}else if(off+size >= contentLength){
		if(off < tmpBuf.size())
			tmpBuf.erase(off);
		tmpBuf += dataStr;
		if(tmpBuf.size() > off+size)
			tmpBuf.erase(off+size);
		bytes_written = size;
	}else{
		bytes_written = size;
		tmpBuf.replace(off, size, dataStr.substr(0,size));
	}
	ec->put(ino, tmpBuf);
	//printf("\n\n\nwrite:\nino:%d\noff:%d\nrequest length:%d\nactual length:%dlength after:%d\n%s\n%s\n\n\n", ino,off,size,bytes_written,tmpBuf.length(),data, tmpBuf.c_str());
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
	std::list<dirent> dirList;
	bool found;
	yfs_client::inum ino;
	r = lookup(parent, name, found, ino);
	if(found){
		ec->remove(ino);
		std::list<dirent>::iterator iter;
		std::string nameStr = std::string(name);
		readdir(parent, dirList);
		for(iter = dirList.begin(); iter != dirList.end(); ++iter){
			if(iter->name == nameStr){
				dirList.erase(iter);
				break;
			}
		}
	}else r = NOENT; 
	//write back to parent dir
	std::string parentDir = "";
	std::list<dirent>::iterator iter;
	for(iter = dirList.begin(); iter != dirList.end(); ++iter){
		parentDir += filename(iter->inum);
		parentDir += "/";
		parentDir += iter->name;
		parentDir += "/";
	}
	parentDir += "0/";
	ec->put(parent, parentDir);

    return r;
}

