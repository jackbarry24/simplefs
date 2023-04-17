/* fs.c: SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"
#include "sfs/utils.h"

#include <stdio.h>
#include <string.h>

/**
 * Debug FileSystem.
 * @param       disk        Pointer to Disk structure.
 **/
void    fs_debug(Disk *disk) {
    Block block;
    
    /* Read SuperBlock */
    if (disk_read(disk, 0, block.data) == DISK_FAILURE) return;
    
    printf("SuperBlock:\n");
    printf("    magic number is %s\n",
        (block.super.magic_number == MAGIC_NUMBER) ? "valid" : "invalid");
    printf("    %u blocks\n"         , block.super.blocks);
    printf("    %u inode blocks\n"   , block.super.inode_blocks);
    printf("    %u inodes\n"         , block.super.inodes);

    /* Read Inodes */
    Block iBlock;
    for(int i = 1; i <= block.super.inode_blocks; i++){
        if(disk_read(disk, i, iBlock.data) ==  DISK_FAILURE) continue;
        for(int j = 0; j < INODES_PER_BLOCK; j++){
            if(!iBlock.inodes[j].valid) continue;
            printf("Inode %d:\n", (i-1)*INODES_PER_BLOCK + j);
            printf("    size: %d bytes\n", iBlock.inodes[j].size);
            //load current inode number into inode struct
            Inode inode;
            if(!fs_load_save_inode(disk, (i-1)*INODES_PER_BLOCK + j, &inode, 0)) continue;
            //display inode attributes
            direct_blocks(&inode);
            if(inode.indirect){
                printf("    indirect block: %d\n", inode.indirect);
                indirect_blocks(&inode, disk);
            }
        }
    }
}

/**
 * count number of blocks directly pointed to by inode
 *
 * @param       inode   Pointer to a Inode structure.
 **/
void direct_blocks(Inode* inode){
    printf("    direct blocks:");
    for(int i = 0; i < POINTERS_PER_INODE; i++){
        if(inode->direct[i]) printf(" %d", inode->direct[i]);
    }
    printf("\n");
}

/**
 * count number of blocks pointed to by the indirect pointer block
 *
 * @param       inode   Pointer to the Inode structure.
 * @param       disk    Pointer to the Disk structure.
 **/
void indirect_blocks(Inode* inode, Disk* disk){
    if(!inode->indirect) return;
    printf("    indirect data blocks:");
    Block block;
    if(disk_read(disk, inode->indirect, block.data) == DISK_FAILURE) return;
    for(int k = 0; k < POINTERS_PER_BLOCK; k++){
        if (block.pointers[k]) printf(" %d", block.pointers[k]);       
    }   
    printf("\n");
}

/**
 * Format Disk.
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not all disk operations were successful.
 **/
bool    fs_format(FileSystem *fs, Disk *disk) {
    if(fs->disk) return false;

    Block block = {{0}};
    block.super.magic_number = MAGIC_NUMBER;
    block.super.blocks = disk->blocks;
    //calcuate # of inode blocks needed
    int remainder = (disk->blocks % 10) ? 1 : 0;
    block.super.inode_blocks = disk->blocks / 10 + remainder;
    block.super.inodes = block.super.inode_blocks * INODES_PER_BLOCK;
    if(disk_write(disk, 0, block.data) == DISK_FAILURE) return false;
    
    //set rest of blocks to 0 (create a zeroed block and set rest of blocks to it)
    Block zero_block = {{0}};
    for(int i = 1; i < disk->blocks; i++){
        if(disk_write(disk, i, zero_block.data) == DISK_FAILURE) continue;
    }
    return true;
}

/**
 * Mount specified FileSystem to given Disk.
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not the mount operation was successful.
 **/
bool    fs_mount(FileSystem *fs, Disk *disk) {
    if(fs->disk) return false;

    Block sb;
    if(disk_read(disk, 0, sb.data) == DISK_FAILURE) return false;
    
    if(disk->blocks != sb.super.blocks) return false;
    int remainder = (disk->blocks % 10) ? 1 : 0;
    int disk_inode_blocks = disk->blocks / 10 + remainder;
    if(disk_inode_blocks != sb.super.inode_blocks) return false;
    if(disk_inode_blocks * INODES_PER_BLOCK != sb.super.inodes) return false;
    if(sb.super.magic_number != MAGIC_NUMBER) return false;

    fs->disk = disk;
    fs->meta_data = sb.super;

    //free block bitmap init
    fs->free_blocks = malloc(disk->blocks);
    if(!fs->free_blocks) return false;
    memset(fs->free_blocks, 1, disk->blocks);
    fs_init_free_block_map(fs);
    return true;
}

/**
 * Initialize the free block bitmap
 * @param       fs      Pointer to FileSystem structure.
 **/
void fs_init_free_block_map(FileSystem *fs){
    //set superblock to false
    fs->free_blocks[0] = false;

    Block block;
    for(int i = 1; i <= fs->meta_data.inode_blocks; i++){
        fs->free_blocks[i] = false;
        if(disk_read(fs->disk, i, block.data) == DISK_FAILURE) continue;
        for(int j = 0; j < INODES_PER_BLOCK; j++){
            if(!block.inodes[j].valid) continue;
            //set all direct blocks to false
            for(int k = 0; k < POINTERS_PER_INODE; k++){
                fs->free_blocks[block.inodes[j].direct[k]] = false;
            }
            //set pointer block and indirect blocks to false
            if(!block.inodes[j].indirect) continue;
            else fs->free_blocks[block.inodes[j].indirect] = false;
            Block indirect = {{0}};
            if(disk_read(fs->disk, block.inodes[j].indirect, indirect.data) == DISK_FAILURE) continue;
            for(int k = 0; k < POINTERS_PER_BLOCK; k++){
                fs->free_blocks[indirect.pointers[k]] = false;
            }
        }
    }
}

/**
 * Unmount FileSystem from internal Disk.
 * @param       fs      Pointer to FileSystem structure.
 **/
void    fs_unmount(FileSystem *fs) {
    fs->disk = NULL;
    free(fs->free_blocks);
    fs->free_blocks = NULL;
}

/**
 * Allocate an Inode in the FileSystem Inode table.
 * @param       fs      Pointer to FileSystem structure.
 * @return      Inode number of allocated Inode.
 **/
ssize_t fs_create(FileSystem *fs) {
    Block block;
    for(int i = 1; i <= fs->meta_data.inode_blocks; i++){
        if(disk_read(fs->disk, i, block.data) == DISK_FAILURE) continue;
        for(int j = 0; j < INODES_PER_BLOCK; j++){
            if(block.inodes[j].valid == 0) {
                block.inodes[j].valid = 1;
                block.inodes[j].size = 0;
                if(disk_write(fs->disk, i, block.data) == DISK_FAILURE) return -1;
                return (i-1) * INODES_PER_BLOCK + j;
            }
        }
    }
    return -1;
}

/**
 * Remove Inode and associated data from FileSystem by doing the following.
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Whether or not removing the specified Inode was successful.
 **/
bool    fs_remove(FileSystem *fs, size_t inode_number) {
    Inode inode;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 0)) return false;

    Block indirect;
    if(inode.indirect){
        if(disk_read(fs->disk, inode.indirect, indirect.data) == DISK_FAILURE) return false;
    }
    Block zero_block = {{0}};
    
    uint32_t block_pointer = 0;
    for(int i = 0; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){
        if(i < POINTERS_PER_INODE){
            if(!inode.direct[i]) continue;
            block_pointer = inode.direct[i];
            fs->free_blocks[block_pointer] = true;
        }else{
            if(!inode.indirect) break;
            block_pointer = indirect.pointers[i - POINTERS_PER_INODE];
            fs->free_blocks[block_pointer] = true;
        }
        if(disk_write(fs->disk, block_pointer, zero_block.data) == DISK_FAILURE) continue;
    }
    if(inode.indirect) fs->free_blocks[inode.indirect] = true;
    inode.valid = 0;
    inode.size = 0;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 1)) return false;
    return true;
}

/**
 * save or load an inode
 * @param       disk            Pointer to Disk structure.
 * @param       inode_number    Inode to remove.
 * @param       node            node structure to load inode into or save to inode
 * @param       mode            0 for load, 1 for save
 * @return      load or save a specified inode into inode table or vice versa
 **/

bool fs_load_save_inode(Disk* disk, size_t inode_number, Inode *node, int mode){
    size_t block_num = inode_number / INODES_PER_BLOCK + 1;
    size_t offset = inode_number % INODES_PER_BLOCK;
    Block block;
    if(disk_read(disk, block_num, block.data) == DISK_FAILURE) return false;
    if (mode == 0) {
        if(!block.inodes[offset].valid) return false;
        *node = block.inodes[offset];
    }else {
        block.inodes[offset] = *node;
        if(disk_write(disk, block_num, block.data) == DISK_FAILURE) return false;
    }
    return true;
}

/**
 * Return size of specified Inode.
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Size of specified Inode (-1 if does not exist).
 **/
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {
    Inode inode;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 0)) return -1;
    return inode.size;
}

/**
 * Read from the specified Inode into the data buffer exactly length bytes
 * beginning from the specified offset.
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to read data from.
 * @param       data            Buffer to copy data to.
 * @param       length          Number of bytes to read.
 * @param       offset          Byte offset from which to begin reading.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    Inode inode;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 0)) return -1;
    if(offset >= inode.size) return -1;

    size_t      bytes_read = 0, bytes_to_copy;
    size_t      bytes_to_read = min(length, inode.size - offset);
    ssize_t     start_block = offset / BLOCK_SIZE;
    size_t      block_offset = offset % BLOCK_SIZE;
    Block       indirect, block;

    if(inode.indirect){
        if(disk_read(fs->disk, inode.indirect, indirect.data) == DISK_FAILURE) return -1;
    }
    
    uint32_t block_pointer = 0;
    for(int i = start_block; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){
        bytes_to_copy = min(BLOCK_SIZE - block_offset, bytes_to_read - bytes_read);
        if (i < POINTERS_PER_INODE) block_pointer = inode.direct[i];
        else{
            if(!inode.indirect) return -1;
            block_pointer = indirect.pointers[i - POINTERS_PER_INODE];
        }
        if(!block_pointer) return -1;
        if(disk_read(fs->disk, block_pointer, block.data) == DISK_FAILURE) return -1;
        memcpy(data + bytes_read, block.data + block_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
        block_offset = 0;
        if(bytes_read == bytes_to_read) return bytes_read;
    }
    return bytes_read;
}

/**
 * Write to the specified Inode from the data buffer exactly length bytes
 * beginning from the specified offset.
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to write data to.
 * @param       data            Buffer with data to copy
 * @param       length          Number of bytes to write.
 * @param       offset          Byte offset from which to begin writing.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    Inode inode;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 0)) return -1;

    size_t      bytes_written = 0, bytes_to_copy;
    ssize_t     start_block = offset / BLOCK_SIZE;
    size_t      block_offset = offset % BLOCK_SIZE;
    uint32_t    block_pointer = 0;
    Block       indirect = {{0}}, block = {{0}}, zero_block = {{0}};
    bool        inode_changes = false;

    if(inode.indirect){
        if(disk_read(fs->disk, inode.indirect, indirect.data) == DISK_FAILURE) return -1;
    }

    for(int i = start_block; i < POINTERS_PER_BLOCK + POINTERS_PER_INODE; i++){
        bytes_to_copy = min(BLOCK_SIZE - block_offset, length - bytes_written);
        if (i < POINTERS_PER_INODE) block_pointer = inode.direct[i];
        else{
            //allocate indirect block if needed
            if(!inode.indirect){
                if((block_pointer = find_free_block(fs)) == -1) break;
                inode.indirect = block_pointer;
                if(disk_write(fs->disk, inode.indirect, zero_block.data) == DISK_FAILURE) return -1;
                if(disk_read(fs->disk, inode.indirect, indirect.data) == DISK_FAILURE) return -1;
            }
            block_pointer = indirect.pointers[i - POINTERS_PER_INODE];   
        }
        //allocate block if needed
        if(!block_pointer){
            if((block_pointer = find_free_block(fs)) == -1) break;
            if(i < POINTERS_PER_INODE) inode.direct[i] = block_pointer;
            else indirect.pointers[i - POINTERS_PER_INODE] = block_pointer;
            inode_changes = true;
        }
        if(disk_read(fs->disk, block_pointer, block.data) == DISK_FAILURE) return -1;
        memcpy(block.data + block_offset, data + bytes_written, bytes_to_copy);
        if(disk_write(fs->disk, block_pointer, block.data) == DISK_FAILURE) return -1;
        
        //only rewrite inode block if new block has been allocated
        if(inode.indirect && inode_changes){
            if(disk_write(fs->disk, inode.indirect, indirect.data) == DISK_FAILURE) return -1;
        }
        bytes_written += bytes_to_copy;
        block_offset = 0;
        inode_changes = false;
        if(bytes_written == length) break;
    }
    inode.size += bytes_written;
    if(!fs_load_save_inode(fs->disk, inode_number, &inode, 1)) return -1;
    return bytes_written;
}

/**
 * Find a free block and return its block number
 * @param       fs  Pointer to FileSystem structure.
 * @return      Pointer to free block
 **/
ssize_t find_free_block(FileSystem *fs){
    for(ssize_t i = 0; i < fs->meta_data.blocks; i++){
        if(fs->free_blocks[i]){
            fs->free_blocks[i] = false;
            return i;
        }
    }
    return -1;
}