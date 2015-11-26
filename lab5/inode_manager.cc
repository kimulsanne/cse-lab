#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
	if(id < 0 || id > BLOCK_NUM-1 || buf == NULL) return;
	memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
	if(id < 0 || id > BLOCK_NUM-1) return;
	memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
	blockid_t id = IBLOCK(INODE_NUM, sb.nblocks) + 1;
	char tmpBlock[BLOCK_SIZE];
	char tmpBuf;
	for(; id< BLOCK_NUM; ++id){
		read_block(BBLOCK(id), tmpBlock);
		tmpBuf = tmpBlock[id % (BLOCK_SIZE*8) / 8];
		if(((int)tmpBuf & (1 << (7 - id%8))) == 0){
			tmpBuf |= (1 << (7 - id%8));
			tmpBlock[id % (BLOCK_SIZE*8) / 8] = tmpBuf;
			write_block(BBLOCK(id), tmpBlock);
			break;
		}
	}
	write_block(id, "");
	return id;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
	if(id < IBLOCK(INODE_NUM, sb.nblocks)+1 || id > BLOCK_NUM-1) return;
	char tmpBlock[BLOCK_SIZE];
	char tmpBuf;
	read_block(BBLOCK(id), tmpBlock);
	tmpBuf = tmpBlock[id % (BLOCK_SIZE*8) / 8];
	tmpBuf &= (~(1 << (7 - id%8)));
	tmpBlock[id % (BLOCK_SIZE*8) / 8] = tmpBuf;
	write_block(BBLOCK(id), tmpBlock);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */
	uint32_t inum = 1;
	struct inode ino;
	char tmpBlock[BLOCK_SIZE];
	char tmpBuf;
	ino.type = type;
	ino.size = 0;
	ino.atime = time(0);
	ino.mtime = time(0);
	ino.ctime = time(0);
	for(; inum<= INODE_NUM; ++inum){
		bm->read_block(BBLOCK(IBLOCK(inum, bm->sb.nblocks)), tmpBlock);
		tmpBuf = tmpBlock[IBLOCK(inum, bm->sb.nblocks) % (BLOCK_SIZE*8)/8];
		if(((int)tmpBuf & (1 << (7-IBLOCK(inum, bm->sb.nblocks)%8))) == 0){
			tmpBuf |= (1 << (7 - IBLOCK(inum, bm->sb.nblocks) % 8));
			tmpBlock[IBLOCK(inum, bm->sb.nblocks)%(BLOCK_SIZE*8)/8]= tmpBuf;
			bm->write_block(BBLOCK(IBLOCK(inum, bm->sb.nblocks)), tmpBlock);
			break;
		}
	}
	put_inode(inum, &ino);
	return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
	char tmpBlock[BLOCK_SIZE];
	char tmpBuf;
	bm->read_block(BBLOCK(IBLOCK(inum, bm->sb.nblocks)), tmpBlock);
	tmpBuf = tmpBlock[IBLOCK(inum, bm->sb.nblocks) % (BLOCK_SIZE*8) / 8];
	if(((int)tmpBuf & (1 << (7-IBLOCK(inum, bm->sb.nblocks)%8))) == 0) return;
	//set related bit in bitMap to zero
	tmpBuf &= (~(1 << (7-IBLOCK(inum, bm->sb.nblocks)%8)));
	tmpBlock[IBLOCK(inum, bm->sb.nblocks)%(BLOCK_SIZE*8) / 8] = tmpBuf;
	bm->write_block(BBLOCK(IBLOCK(inum, bm->sb.nblocks)), tmpBlock);
	//clean the ino;
	struct inode ino;
	ino.atime = time(0);
	ino.mtime = time(0);
	ino.size = 0;
	ino.type = 0;
	put_inode(inum, &ino);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
	struct inode *ino = get_inode(inum);
	ino->atime = time(0);
	put_inode(inum, ino);
	if(ino->type == 0 || ino->size == 0){
		free(ino);
		*size = 0;
		return;
	}
	*size = ino->size;
	unsigned int nBlock = 0;
	if(ino->size != 0) nBlock = (ino->size-1)/BLOCK_SIZE + 1;
	*buf_out = (char*)malloc(nBlock * BLOCK_SIZE * sizeof(char*));

	if(nBlock <= NDIRECT){
		for(unsigned int i= 0; i< nBlock; ++i){
			bm->read_block(ino->blocks[i], *buf_out + i*BLOCK_SIZE);
		}
	}else{
		for(unsigned int i= 0; i< NDIRECT; ++i){
			bm->read_block(ino->blocks[i], *buf_out + i*BLOCK_SIZE);
		}
		blockid_t tmpBlock[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)&tmpBlock);
		for(unsigned int i= NDIRECT; i< nBlock; ++i){
			bm->read_block(tmpBlock[i-NDIRECT], *buf_out +i*BLOCK_SIZE);
		}
	}
	free(ino);
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
	struct inode *ino = get_inode(inum);
	ino->atime = time(0);
	ino->mtime = time(0);
	ino->ctime = time(0);
	unsigned int nOldBlock = 0;
	unsigned int nNewBlock = 0;
	if(ino->size != 0) nOldBlock = (ino->size - 1)/BLOCK_SIZE + 1;
	if(size != 0) nNewBlock = (size - 1)/BLOCK_SIZE + 1;

	ino->size = size;

	if(nNewBlock > MAXFILE) return;
	//alloc new memery if necessary
	if(nNewBlock > nOldBlock){
		if(nNewBlock <= NDIRECT){
			for(unsigned int i= nOldBlock; i< nNewBlock; ++i){
				ino->blocks[i] = bm->alloc_block();
			}
		}else if(nOldBlock > NDIRECT){
			blockid_t tmpBlock[NINDIRECT];
			bm->read_block(ino->blocks[NDIRECT], (char*)tmpBlock);
			for(unsigned int i= nOldBlock; i< nNewBlock; ++i){
				tmpBlock[i-NDIRECT] = bm->alloc_block();
			}
			bm->write_block(ino->blocks[NDIRECT], (const char*)tmpBlock);
		}else{
			for(unsigned int i= nOldBlock; i< NDIRECT; ++i){
				ino->blocks[i] = bm->alloc_block();
			}
			ino->blocks[NDIRECT] = bm->alloc_block();
			blockid_t tmpBlock[NINDIRECT];
			for(unsigned int i= NDIRECT; i< nNewBlock; ++i){
				tmpBlock[i-NDIRECT] = bm->alloc_block();
			}
			bm->write_block(ino->blocks[NDIRECT], (const char*)tmpBlock);
		}
	}
	//copy
	if(nNewBlock <= NDIRECT){
		for(unsigned int i= 0; i< nNewBlock; ++i){
			bm->write_block(ino->blocks[i], buf+BLOCK_SIZE*i);
		}
	}else{
		for(unsigned int i= 0; i< NDIRECT; ++i){
			bm->write_block(ino->blocks[i], buf+BLOCK_SIZE*i);
		}
		blockid_t tmpBlock[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)tmpBlock);
		for(unsigned int i= NDIRECT; i< nNewBlock; ++i){
			bm->write_block(tmpBlock[i-NDIRECT], buf+BLOCK_SIZE*i);
		}
	}
	//free if necessary
	if(nNewBlock < nOldBlock){
		if(nOldBlock <= NDIRECT){
			for(unsigned int i= nNewBlock; i< nOldBlock; ++i){
				bm->free_block(ino->blocks[i]);
			}
		}else if(nNewBlock > NDIRECT){
			blockid_t tmpBlock[NINDIRECT];
			bm->read_block(ino->blocks[NDIRECT], (char*)tmpBlock);
			for(unsigned int i= nNewBlock; i< nOldBlock; ++i){
				bm->free_block(tmpBlock[i-NDIRECT]);
			}
			bm->write_block(ino->blocks[NDIRECT], (const char*)tmpBlock);
		}else{
			for(unsigned int i= nNewBlock; i< NDIRECT; ++i){
				bm->free_block(ino->blocks[i]);
			}
			blockid_t tmpBlock[NINDIRECT];
			bm->read_block(ino->blocks[NDIRECT], (char*)tmpBlock);
			for(unsigned int i= NDIRECT; i< nOldBlock; ++i){
				bm->free_block(tmpBlock[i-NDIRECT]);
			}
			bm->free_block(ino->blocks[NDIRECT]);
		}
	}
	put_inode(inum, ino);
	free(ino);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
	struct inode *ino = get_inode(inum);
	if(ino == NULL) return;
	a.type  = ino->type;
	a.atime = ino->atime;
	a.mtime = ino->mtime;
	a.ctime = ino->ctime;
	a.size  = ino->size;
	
	free(ino);
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*i
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
	struct inode *ino = get_inode(inum);
	unsigned int nBlock = 0;
	if(ino->size != 0) nBlock = (ino->size-1)/BLOCK_SIZE+1;
	if(nBlock <= NDIRECT){
		for(unsigned int i= 0; i< nBlock; ++i){
			bm->free_block(ino->blocks[i]);
		}
	}else{
		for(unsigned int i= 0; i< NDIRECT; ++i){
			bm->free_block(ino->blocks[i]);
		}
		blockid_t tmpBlock[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)&tmpBlock);
		for(unsigned int i= NDIRECT; i< nBlock; ++i){
			bm->free_block(tmpBlock[i-NDIRECT]);
		}
		bm->free_block(ino->blocks[NDIRECT]);
	}
	free_inode(inum);
	free(ino);
}

void inode_manager::set_log_inode(uint32_t inum, uint32_t type){
	struct inode ino;
	ino.type = type;
	ino.size = 0;
	ino.atime = time(0);
	ino.mtime = time(0);
	ino.ctime = time(0);
	put_inode(inum, &ino);
}
