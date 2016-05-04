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

// int checkBlockAddr(uint blockAddr)
// {
//     if()
// }

int checkDirectoryForEntry(struct dirent * directory, char * name)
{
    int check = 0;
    int i = 0;
    for (i = 0; i < NUMDIRENTPERBLOCK; ++i){
        if(strcmp(directory[i].name, name) == 0){
            printf("We just found the dir entry %s\n", name);
            check = 1;
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

    // int rc;
    // char buf[BSIZE];
    // rc = read(fd, buf, BSIZE);
    // assert(rc  == BSIZE);
    // rc = read(fd, buf, BSIZE);
    // assert(rc  == BSIZE);


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

    int dotCheck = 0;
    int dotdotCheck = 0;
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
            }
            // printf("Check 2: InDirect Blocks...\n");
            indirectDataBlockAddresses = img + (dataBlockAddresses[NDIRECT] * BSIZE);
            for (j = 0; j < BSIZE / (sizeof(uint)); ++j)
            {
                if(indirectDataBlockAddresses[j] == 0){
                    printf("j = %d %s\n",j, "unused indirect data block");
                    break;
                }
                checkDataBlockAddress(sb, indirectDataBlockAddresses[j]);
            }
        }

        // check3 check for root directory
        if (i == ROOTINO)
        {
            // printf("Check 3...\n");
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
            
            for (j = 0; j < NDIRECT + 1; ++j)
            {
                if(dataBlockAddresses[j] == 0){
                    printf("j = %d %s\n",j, "unused data block");
                    break;
                }
                currDirectory = img + (dataBlockAddresses[j] * BSIZE);

                // checkDirectoryForEntry(currDirectory, ".");

                // checkDirectoryForEntry(currDirectory, "..");

                for (k = 0; k < BSIZE / (sizeof(uint)); ++k){
                    if(strcmp(currDirectory[k].name, ".") == 0){
                        dotCheck = 1;
                    }
                    //check5: check that parent dir
                    if(strcmp(currDirectory[k].name, "..") == 0){
                        dotdotCheck = 1;
                        int parentDirCheck = 0;
                        
                        struct dinode * parentNode = img + (currDirectory[k].inum * BSIZE);
                        struct dirent * parentDirEntry;

                        int l = 0;
                        for (l = 0; l < NDIRECT; ++l){   
                            parentDirEntry = img + (parentNode->addrs[l] * BSIZE);
                            int m = 0;

                            for (m = 0; m < BSIZE / (sizeof(uint)); ++m){
                                if(parentDirEntry[m].inum == i) {
                                    parentDirCheck = 1;
                                    break;
                                }
                            }
                            if(parentDirCheck) {
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
                }
                if(dotCheck && dotdotCheck){
                    break;
                }

            }
            if(!dotCheck || !dotdotCheck){
                fprintf(stderr, "%s\n", "directory not properly formatted");
                exit(1);
            }
            else{
                printf("hello im Felipe in %i\n",i);
            }
        }

        inode++;
    }

    // check5: 




    if(close(fd) < 0){
        fprintf(stderr,"error closing image\n");
        return 1;
    }

    return 0;
}
