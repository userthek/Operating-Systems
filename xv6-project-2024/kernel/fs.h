// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040
#define NDIRECT 11 //*edited* we reduce as asked the direct blocks to 11
#define NINDIRECT (BSIZE / sizeof(uint))//num of block adresses that an indirect block can hold(256)
#define NDOUBLY_INDIRECT NINDIRECT*NINDIRECT//*added to implement the doubly indirect struct blocks* 
#define MAXFILE (NDIRECT + NINDIRECT + NDOUBLY_INDIRECT)//*edited to accomodate the doubly indirect blocks as well in the max num of blocks in a file* 

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Attention!! It was initially NDIRECT+1, and it seems like I did not respect
							// the requiremnts of the exercise which explicitly say that we should not modify the on disk
							//inode size. Yet I have respected them because I have modified the macro NDIRECT from 12 to 11. 
							// So there is an equilibrium and the size of the array remains unmodified(ndirect' = ndirect+1 ,
							// so sizeof(addrs)= ndirect' + 1 = ndirect+1 + 1= ndirect + 2 ) 
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

