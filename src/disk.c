/* disk.c: SimpleFS disk emulator */

#include "sfs/disk.h"
#include "sfs/logging.h"

#include <fcntl.h>
#include <unistd.h>


bool    disk_sanity_check(Disk *disk, size_t blocknum, const char *data);

/**
 *
 * Opens disk at specified path with the specified number of blocks.
 * @param       path        Path to disk image to create.
 * @param       blocks      Number of blocks to allocate for disk image.
 *
 * @return      Pointer to newly allocated and configured Disk structure (NULL
 *              on failure).
 **/
Disk *	disk_open(const char *path, size_t blocks) {
    if(blocks < 0) return NULL;

    Disk* disk = calloc(1, sizeof(Disk));
    if(!disk) return NULL;

    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        free(disk);
        return NULL;
    } 
    
    disk->fd = fd;
    disk->blocks = blocks;
    
    if(ftruncate(fd, blocks * BLOCK_SIZE) == -1) {
        disk_close(disk);
        return NULL;
    }
    return disk;
}

/**
 * Close disk structure.
 * @param       disk        Pointer to Disk structure.
 */
void	disk_close(Disk *disk) {
    close(disk->fd);
    printf("disk block reads:  %ld", disk->reads);
    printf("disk block writes: %ld", disk->reads);
    free(disk);
}

/**
 * Read data from disk at specified block into data buffer.
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes read.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_read(Disk *disk, size_t block, char *data) {
    if(!disk_sanity_check(disk, block, data)) return DISK_FAILURE;

    if(lseek(disk->fd, block * BLOCK_SIZE, SEEK_SET) == -1) return DISK_FAILURE;
  
    disk->reads++;
    if ((read(disk->fd, data, BLOCK_SIZE)) != BLOCK_SIZE) return DISK_FAILURE;
    return BLOCK_SIZE;
}

/**
 * Write data to disk at specified block from data buffer.
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes written.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_write(Disk *disk, size_t block, char *data) {
    if(!disk_sanity_check(disk, block, data)) return DISK_FAILURE;

    if(lseek(disk->fd, block * BLOCK_SIZE, SEEK_SET) == -1) return DISK_FAILURE;

    disk->writes++;
    if((write(disk->fd, data, BLOCK_SIZE)) != BLOCK_SIZE) return DISK_FAILURE;
    return BLOCK_SIZE;
}


/**
 * Perform sanity check before read or write operation.
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Whether or not it is safe to perform a read/write operation
 *              (true for safe, false for unsafe).
 **/
bool    disk_sanity_check(Disk *disk, size_t block, const char *data) {
    if(!disk || !data || block >= disk->blocks) return false;
    return true;
}