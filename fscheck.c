/* Program 5 fscheck
 * Felipe Gutierrez - Student ID: 9067242934 
 * Blake Nigh - Student ID: 9067207283
 * April 25th, 2016
 */

/**
 * Grading comments
 * The program will be compiled with:
 *     gcc -O -Wall -o fastsort fastsort.c
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>     /* assert */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#define T_UNALLOCATED 0
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

// On-disk file system format.
// Both the kernel and user programs use this header file.

// Block 0 is unused.
// Block 1 is super block.
// Inodes start at block 2.

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)


// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#define DIRENTSIZE 16
#define NUMDIRENTPERBLOCK (int) (BSIZE / DIRENTSIZE)



int * blockNumToBitAddr(struct superblock *sb, int blockNum)
{
    int blockIndex = (blockNum - (sb->size - sb->nblocks));
    uint * bitAddr = (uint *) (blockIndex + ((sb->ninodes / IPB) + 3) * BSIZE); 

    return bitAddr;
}

/**
* Input:    * Pointer to currDirectory
*           * name to look for
* Output: 
*           * dirEntry is a copy of the directory entry we found (if found)
* returns 1 if found 0 otherwise.
*/
int checkDirectoryForEntry(struct dirent * directory, char * name, struct dirent * dirEntry)
{
    int check = 0;
    int i = 0;
    for (i = 0; i < NUMDIRENTPERBLOCK; ++i){
        if(strcmp(directory[i].name, name) == 0){
            printf("We just found the dir entry %s\n", name);
            strcpy(dirEntry->name, directory[i].name);
            dirEntry->inum = directory[i].inum;
            check = 1;
            break;
        }
    }
    return check;
}

int checkDirectoryForINum(struct dirent * directory, int inum, struct dirent * dirEntry)
{
    int check = 0;
    int i = 0;
    for (i = 0; i < NUMDIRENTPERBLOCK; ++i){
        // printf("Checking for Inum for %d: [%d]\n", i, directory[i].inum);

        if(directory[i].inum == inum){
            printf("We just found the dir inum %d\n", (int) inum);
            strcpy(dirEntry->name, directory[i].name);
            dirEntry->inum = directory[i].inum;
            check = 1;
            break;
        }
    }
    return check;
}

void checkDataBlockAddress(struct superblock *sb, uint addr)
{
    if(addr > sb->size || 
       addr < (sb->size - sb->nblocks)){
        fprintf(stderr, "%s\n", "bad address in inode.");
        exit(1);
    }
}

/**
 * File System checker. 
 */
int main(int argc, char **argv)
{
    printf("Executing fscheck...\n");
    printf("size of int = %d...\n",(int) sizeof(int));
    printf("size of uint = %d...\n",(int) sizeof(uint));
    printf("size of short = %d...\n",(int) sizeof(short));

    char * imgPath;
    int fd;
    struct stat imgStat;
    void * img;

    if(argc != 2){
        fprintf(stderr,"image not found.\n");
        return 1;
    }

    imgPath = argv[1];
    printf("Input Image Path = %s \n", imgPath);
    
    fd = open(imgPath, O_RDONLY);
    if(fd < 0){
        fprintf(stderr,"image not found.\n");
        return 1;
    }
    if(fstat(fd, &imgStat) < 0){
        fprintf(stderr, "image not found: fstat failed\n");
        return 1;
    }
    printf("Input Image Size = %zd \n", imgStat.st_size);


    img = mmap(NULL, imgStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0); 
    if(img == MAP_FAILED){
        fprintf(stderr, "mmap failed\n");
    }
    struct superblock * sb;
    sb = (struct superblock * )(img + BSIZE);
    printf("%d %d %d\n", sb->size, sb->nblocks, sb->ninodes );
    
    int i, j, k, m;
    struct dinode * inode;
    uint * dataBlockAddresses;
    uint * indirectDataBlockAddresses;
    struct dirent * currDirectory; // List of dirent
    struct dirent currDirEntry;
    struct dirent parentDirEntry;
    struct dirent tmpEntry;
    struct dirent * parentDirectory;
    struct dinode * parentNode;
    int dotCheck, dotdotCheck, parentDirCheck;
    struct dinode ** inUseINodes = malloc(sb->ninodes * sizeof(struct dinode *));
    int inUseCounter = 0;

    inode = (struct dinode *)(img + 2*BSIZE);

    printf("Size of dirent %d, BSIZE/dirent = %d \n", (int) sizeof(struct dirent), (int) BSIZE/sizeof(struct dirent));
    for (i = 0; i < (int) sb->ninodes; ++i) {
        // printf("%d type: %d\n",i, inode->type);
        dataBlockAddresses = inode->addrs;

        // check1
        if(inode->type != T_UNALLOCATED && 
           inode->type != T_FILE &&
           inode->type != T_DIR &&
           inode->type != T_DEV)
        {
            // printf("Check 1...\n");
            fprintf(stderr,"bad inode.");
            exit(1);
        }

        // check2
        if(inode->type == T_FILE ||
           inode->type == T_DIR ||
           inode->type == T_DEV)
        {
            // printf("Check 2: Direct Blocks...\n");
            for(j = 0; j < NDIRECT + 1; ++j)
            {
                if(dataBlockAddresses[j] == 0){
                    printf("j = %d %s\n",j, "unused data block");
                    break;
                }
                checkDataBlockAddress(sb, dataBlockAddresses[j]);
                if(j == NDIRECT){
                    // printf("Check 2: InDirect Blocks...\n");
                    indirectDataBlockAddresses = img + (dataBlockAddresses[NDIRECT] * BSIZE);
                    for (k = 0; k < BSIZE / (sizeof(uint)); ++k){

                        if(indirectDataBlockAddresses[k] == 0){
                            printf("k = %d %s\n",k, "unused indirect data block");
                            break;
                        }
                        checkDataBlockAddress(sb, indirectDataBlockAddresses[k]);
                    }
                }
            }
            inUseINodes[inUseCounter] = inode; 
            inUseCounter++;
            //printf("inode %d in use, inUseCounter = %d\n",i,inUseCounter);
        }

        // check3 check for root directory
        if (i == ROOTINO)
        {
            if(inode->type != T_DIR){
                fprintf(stderr, "%s\n", "root directory does not exist.");
                exit(1);
            }
        }

        // check4: check that dirs containb . and ..
        if (inode->type == T_DIR)
        {
            printf("Check 4...\n");
            dotCheck = 0;
            dotdotCheck = 0;
            parentDirCheck = 0;
            
            for (j = 0; j < NDIRECT + 1; ++j)
            {
                if(dataBlockAddresses[j] == 0){
                    printf("j = %d %s\n",j, "unused data block");
                    break;
                }
                currDirectory = img + (dataBlockAddresses[j] * BSIZE);

                dotCheck = checkDirectoryForEntry(currDirectory, ".", &currDirEntry);
                // After this line dirEntry will contain the parent directory if it was found
                dotdotCheck = checkDirectoryForEntry(currDirectory, "..", &parentDirEntry);

                if(dotCheck && dotdotCheck){
                    break;
                }

            }
            if(!dotCheck || !dotdotCheck){
                fprintf(stderr, "%s\n", "directory not properly formatted");
                exit(1);
            }
            else{
                printf("hello im Felipe in %i: we found . and .. for this DIR\n",i);
            }

            //check5
            parentNode = (img + 2*BSIZE) + (parentDirEntry.inum * sizeof(struct dinode));

            printf("parentDirEntry.name = %s, parentDirEntry.inum = %d\n",parentDirEntry.name,(int)parentDirEntry.inum);
            printf("parentNode.type = %d\n",(int)parentNode->type);

            for (k = 0; k < NDIRECT; ++k) // TODO ask TA about indirect (NDIRECT + 1)
            {
                // printf("Addr of %d: [%d]\n", k, parentNode->addrs[k]);
                parentDirectory = img + (parentNode->addrs[k] * BSIZE);
                parentDirCheck = checkDirectoryForINum(parentDirectory, i, &tmpEntry);
                if(parentDirCheck){
                    break;
                }
            }

            if(!parentDirCheck) {
                fprintf(stderr, "%s\n", "parent directory mismatch.");
                exit(1);
            } else {
                printf("hello im Blake in %i\n",i);
            }
        }

        inode++;
    }

    printf("Printing in use inodes\n");
    int currBitAddress;
    for(i = 0;i < inUseCounter;i++){
        printf("i = %d, inode type = %d\n",i, inUseINodes[i]->type);

        dataBlockAddresses = inUseINodes[i]->addrs;

        //check6
        for (j = 0; j < NDIRECT + 1; ++j){
            if(dataBlockAddresses[j] == 0){
                printf("j = %d %s\n",j, "unused data block");
                break;
            }
            currBitAddress = blockNumToBitAddr(sb, dataBlockAddresses[j]);

        }

    }


    if(close(fd) < 0){
        fprintf(stderr,"error closing image\n");
        return 1;
    }

    return 0;
}
