#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM 0

#define BLOCK_SIZE 1024
#define DIRECT_BLOCK_NUMBER 10
#define INDIRECT_BLOCK_NUMBER BLOCK_SIZE / sizeof(int)
#define DATA_BLOCKS 1024
#define INODE_TABLE_SIZE 50
#define MAX_OPEN_FILES 20
#define MAX_FILE_NAME 40

#define DELAY 5000

#endif
